#include "FileSystemManager.h"

#include <utility>

std::unordered_map<std::string, RBFileMetadata> FileSystemManager::get_files(const std::string& username) {
    std::shared_lock<std::shared_mutex> slock(mutex);
    auto& db = Database::get_instance();
    std::string sql = "SELECT path, hash, last_write_time, size FROM fs WHERE username = ?;";
    auto results = db.query(sql, {username});

    std::unordered_map<std::string, RBFileMetadata> files;

    if (results.empty())
        return files;

    for (auto & [key, value] : results) {

        RBFileMetadata meta;
        if (!value[1].empty())
            meta.set_checksum(std::stoul(value[1]));
        if (!value[2].empty())
            meta.set_last_write_time(std::stoll(value[2]));
        if (!value[3].empty())
            meta.set_size(std::stoull(value[3]));

        // Add metadata
        files[value[0]] = meta;
    }

    return files;
}

bool FileSystemManager::file_exists(std::string username, const fs::path& path) {
    std::shared_lock<std::shared_mutex> slock(mutex);
    if (!fs::exists(path)) {
        RBLog("FSM >> The path provided doesn't correspond to an existing file", LogLevel::ERROR);
        return false;
    }

    if (!fs::is_regular_file(path)) {
        RBLog("FSM >> The path provided doesn't correspond to a file", LogLevel::ERROR);
        return false;
    }

    auto& db = Database::get_instance();

    std::string sql = "SELECT COUNT(*) FROM fs WHERE username = ? AND path = ?;";
    auto results = db.query(sql, {username, path.string()});

    auto count = std::stoi(results[0][0]);
    if (count == 0) {
        RBLog("FSM >> The file provided exists but is not present in the db", LogLevel::ERROR);
        return false;
    }

    return true;
}

void FileSystemManager::write_file(const std::string& username, const RBRequest& req) {
    std::shared_lock<std::shared_mutex> slock(mutex);
    auto& file_segment = req.file_segment();

    const std::string& req_path = file_segment.path();
    if (req_path.find("..") != std::string::npos) {
        RBLog("FSM >> The path provided contains '..' (forbidden)", LogLevel::ERROR);
        throw RBException("forbidden_path");
    }

    // weakly_canonical normalizes a path (even if it doesn't correspond to an existing one)
    auto path = root / username / fs::path(req_path).lexically_normal();
    if (path.filename().empty()) {
        RBLog("FSM >> The path provided is not formatted as a valid file path", LogLevel::ERROR);
        throw RBException("malformed_path");
    }

    auto segment_id = file_segment.segmentid();
    auto req_normal_path = fs::path(req_path).lexically_normal().string();

    // Check correct segment number from db before writing it
    auto& db = Database::get_instance();
    std::string sql = "SELECT last_segment FROM fs WHERE username = ? AND path = ?;";
    auto results = db.query(sql, {username, req_normal_path});

    auto last_segment = 0;
    if (!results.empty())
        last_segment = std::stoi(results[0][0]);

    // Skip this check if segment_id == 0 to allow starting over at any time
    if (segment_id != 0 && segment_id != last_segment + 1)
        throw RBException("wrong_segment");
    
    // Create directories containing the file
    fs::create_directories(path.parent_path());
    
    // Create or overwrite file if it's the first segment (segment_id == 0), otherwise append to file
    auto open_mode = segment_id == 0 ? std::ios::trunc : std::ios::app;
    std::ofstream ofs = std::ofstream(
        path.string(), open_mode | std::ios::binary
    );
    
    if (ofs.is_open()) {
        for (const std::string& datum : file_segment.data())
            ofs << datum;
        ofs.close();
    } else {
        RBLog("FSM >> Cannot open file", LogLevel::ERROR);
        throw RBException("internal_server_error");
    }

    // Save number of written-to-file segments
    if (segment_id == 0) {
        // CHECK Entry automatically replaced on insert if pair (username, path) conflict
        db.query(
            "INSERT INTO fs (username, path, last_segment) VALUES (?, ?, ?);",
            {username, req_normal_path, std::to_string(segment_id)}
        );
    } else {
        db.query(
            "UPDATE fs SET last_segment = ? WHERE username = ? AND path = ?;",
            {std::to_string(segment_id), username, req_normal_path}
        );
    }

    // Stop here if it's not the last segment
    int num_segments = count_segments(file_segment.file_metadata().size());
    if (num_segments != segment_id + 1)
        return;

    // Calculate final checksum
    auto checksum = calculate_checksum(path);
    if (checksum != file_segment.file_metadata().checksum()) {
        // CHECK Clean up file and related db entry
        fs::remove(path);
        db.query(
            "DELETE FROM fs WHERE username = ? AND path = ?;",
            {username, req_normal_path}
        );
        throw RBException("invalid_checksum");
    }

    auto hash = std::to_string(checksum);
    auto lwt_str = std::to_string(file_segment.file_metadata().last_write_time());
    auto size_str = std::to_string(file_segment.file_metadata().size());
    sql = "UPDATE fs SET hash = ?, last_write_time = ?, size = ? WHERE username = ? AND path = ?;";
    
    db.query(sql, {hash, lwt_str, size_str, username, req_normal_path});
}

void FileSystemManager::remove_file(const std::string& username, const RBRequest& req) {
    std::shared_lock<std::shared_mutex> slock(mutex);
    // CHECK
    auto& file_segment = req.file_segment();

    const std::string& req_path = file_segment.path();
    if (req_path.find("..") != std::string::npos) {
        RBLog("FSM >> The path provided contains '..' (forbidden)", LogLevel::ERROR);
        throw RBException("forbidden_path");
    }

    // weakly_canonical normalizes a path (even if it doesn't correspond to an existing one)
    auto path = root / username / fs::path(req_path).lexically_normal();
    if (path.filename().empty()) {
        RBLog("FSM >> The path provided is not formatted as a valid file path", LogLevel::ERROR);
        throw RBException("malformed_path");
    }

    RBLog("FSM >> Deleting file: " + path.string());
    fs::remove(path);
    
    auto req_normal_path = fs::path(req_path).lexically_normal().string();

    auto& db = Database::get_instance();
    db.query(
        "DELETE FROM fs WHERE username = ? AND path = ?;",
        {username, req_normal_path}
    );
}

void FileSystemManager::read_file_segment(const std::string& username, const RBRequest& req, RBResponse& res) {
    auto& file_segment_info = req.file_segment();

    const std::string& req_path = file_segment_info.path();
    if (req_path.find("..") != std::string::npos) {
        RBLog("FSM >> The path provided contains '..' (forbidden)", LogLevel::ERROR);
        throw RBException("forbidden_path");
    }

    // lexically_normal normalizes a path (even if it doesn't correspond to an existing one)
    auto path = root / username / fs::path(req_path).lexically_normal();
    if (path.filename().empty()) {
        RBLog("FSM >> The path provided is not formatted as a valid file path", LogLevel::ERROR);
        throw RBException("malformed_path");
    }

    auto segment_id = file_segment_info.segmentid();
    auto pos = segment_id * RB_MAX_SEGMENT_SIZE;

    std::ifstream ifs(path.string(), std::ios::binary);
    if (!ifs) {
        RBLog("FSM >> Cannot open file \"" + path.string() + "\" for reading", LogLevel::ERROR);
        throw RBException("cannot_read_file");
    }

    ifs.seekg(0, ifs.end);
    int length = ifs.tellg(); // Get length of file

    /*
    RBLog("Path: " + path.string());
    RBLog("Segment: " + std::to_string(segment_id));
    RBLog("Pos: " + std::to_string(pos));
    RBLog("Length: " + std::to_string(length));
    */

    if (pos > length) {
        RBLog("FSM >> Requested segment exceeds file's length", LogLevel::ERROR);
        throw RBException("invalid_read");
    }

    std::vector<char> buffer(RB_MAX_SEGMENT_SIZE, 0);
    ifs.seekg(pos);
    ifs.read(&buffer[0], RB_MAX_SEGMENT_SIZE);
    auto read = ifs.gcount(); // Get the number of characters that have been read
    ifs.close();

    auto file_segment = std::make_unique<RBFileSegment>();
    file_segment->add_data(&buffer[0], read);
    file_segment->set_segmentid(segment_id);
    file_segment->set_path(fs::path(req_path).lexically_normal().string());
    res.set_allocated_file_segment(file_segment.release());
}

std::string FileSystemManager::md5(fs::path path) {
    std::shared_lock<std::shared_mutex> slock(mutex);
    MD5_CTX md5_ctx;
    MD5_Init(&md5_ctx);

    std::ifstream ifs(path.filename().string(), std::ios::binary);

    char buffer[4096];
    while (ifs.read(buffer, sizeof(buffer)) || ifs.gcount())
        MD5_Update(&md5_ctx, buffer, ifs.gcount());

    unsigned char md[MD5_DIGEST_LENGTH];
    MD5_Final(md, &md5_ctx);

    std::string hash = to_string(md);
    RBLog("FSM >> MD5 hash/digest: " + hash);

    return hash;
}

std::string FileSystemManager::to_string(unsigned char* md) {
    std::stringstream ss;

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(md[i]);

    return ss.str();
}

std::string FileSystemManager::get_hash(std::string username, const fs::path& path) {
    auto& db = Database::get_instance();

    std::string sql = "SELECT hash FROM fs WHERE username = ? AND path = ?";

    auto results = db.query(sql, {username, path.string()});
    if (results.empty())
        return "";

    auto hash = results[0][0];

    return hash;
}

std::string FileSystemManager::get_size(std::string username, const fs::path& path) {
    auto& db = Database::get_instance();

    std::string sql = "SELECT size FROM fs WHERE username = ? AND path = ?;";

    auto results = db.query(sql, {username, path.string()});
    if (results.empty())
        return "";

    auto size = results[0][0];

    return size;
}

std::string FileSystemManager::get_last_write_time(std::string username, const fs::path& path) {
    auto& db = Database::get_instance();

    std::string sql = "SELECT last_write_time FROM fs WHERE username = ? AND path = ?;";

    auto results = db.query(sql, {username, path.string()});
    if (results.empty())
        return "";

    auto last_write_time = results[0][0];

    return last_write_time;
}

void FileSystemManager::clear() {
    std::unique_lock<std::shared_mutex> ulock(mutex);
    boost::filesystem::remove_all(root);
}

namespace fs = boost::filesystem;
void recursiveCleanup(fs::path root) {
    for (auto &file: fs::directory_iterator(root)) {
        if (fs::is_directory(file)) {
            recursiveCleanup(file);
            if (fs::is_empty(file)) fs::remove(file);
        }
    }
}

void FileSystemManager::cleanup_empty_folders() {
    std::unique_lock<std::shared_mutex> ul(mutex);
    if (fs::is_directory(root)) recursiveCleanup(root);
}
