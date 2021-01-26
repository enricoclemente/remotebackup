#include "FileSystemManager.h"

#include <utility>

std::unordered_map<std::string, RBFileMetadata> FileSystemManager::get_files(const std::string& username) {
    auto& db = Database::get_instance();
    std::string sql = "SELECT path, hash, last_write_time, size FROM fs WHERE username = ?;";
    auto results = db.query(sql, {username});

    std::unordered_map<std::string, RBFileMetadata> files;

    if (results.empty())
        return files;

    for (auto & [key, value] : results) {
        // Print
        std::cout << "[row " << key << "] ";
        for (auto & col : value) {
            std::cout << col << ", ";
        }
        std::cout << std::endl;

        // Add metadata
        RBFileMetadata meta;
        files[value[0]] = meta;
        if (!value[1].empty())
            meta.set_checksum(std::stoul(value[1]));
        if (!value[2].empty())
            meta.set_last_write_time(std::stoll(value[2]));
        if (!value[3].empty())
            meta.set_size(std::stoull(value[3]));
    }

    return files;
}

bool FileSystemManager::file_exists(std::string username, const fs::path& path) {
    if (!fs::exists(path)) {
        RBLog("The path provided doesn't correspond to an existing file");
        return false;
    }

    if (!fs::is_regular_file(path)) {
        RBLog("The path provided doesn't correspond to a file");
        return false;
    }

    auto& db = Database::get_instance();

    std::string sql = "SELECT COUNT(*) FROM fs WHERE username = ? AND path = ?;";
    auto results = db.query(sql, {username, path.string()});

    auto count = std::stoi(results[0][0]);
    if (count == 0) {
        RBLog("The file provided exists but is not present in the db");
        return false;
    }

    return true;
}

void FileSystemManager::write_file(const std::string& username, const RBRequest& req) {
    auto& file_segment = req.file_segment();

    const std::string& req_path = file_segment.path();
    if (req_path.find("..") != std::string::npos) {
        RBLog("The path provided contains '..' (forbidden)");
        throw RBException("forbidden_path");
    }

    // weakly_canonical normalizes a path (even if it doesn't correspond to an existing one)
    RBLog("req_path: " + req_path);
    auto path = root / username / fs::path(req_path).lexically_normal();

    RBLog("path: " + path.string());
    if (path.filename().empty()) {
        RBLog("The path provided is not formatted as a valid file path");
        throw RBException("malformed_path");
    }

    auto segment_id = file_segment.segmentid();
    auto req_c_path = fs::path(req_path).lexically_normal().string();

    // Check correct segment number from db before writing it
    auto& db = Database::get_instance();
    std::string sql = "SELECT tmp_chunks FROM fs WHERE username = ? AND path = ?;";
    auto results = db.query(sql, {username, req_c_path});

    auto next_segment_id = 0;
    if (!results.empty())
        next_segment_id = std::stoi(results[0][0]);

    // Skip this check if segment_id == 0 to allow starting over at any time
    if (segment_id != 0 && segment_id != next_segment_id)
        throw RBException("wrong_segment");

    // Create directories containing the file
    RBLog("Creating dirs:" + path.string());
    fs::create_directories(path.parent_path());

    // Create or overwrite file if it's the first segment (segment_id == 0), otherwise append to file
    std::ofstream ofs = segment_id == 0
        ? std::ofstream(path.string())
        : std::ofstream(path.string(), std::ios::app);

    if (ofs.is_open()) {
        for (const std::string& datum : file_segment.data())
            ofs << datum;
        ofs.close();
    } else {
        RBLog("Cannot open file");
        throw RBException("internal_server_error");
    }

    // Save number of written-to-file segments
    if (segment_id == 0) {
        // TODO: delete DB entry
        db.query(
            "INSERT INTO fs (username, path, tmp_chunks) VALUES (?, ?, ?);",
            {username, req_c_path, std::to_string(segment_id + 1)}
        );
    } else {
        db.query(
            "UPDATE fs SET tmp_chunks = ? WHERE username = ? AND path = ?;",
            {std::to_string(segment_id + 1), username, req_c_path}
        );
    }

    // Stop here if it's not the last segment
    int num_segments = count_segments(file_segment.file_metadata().size());
    if (num_segments != segment_id + 1)
        return;

    // Calculate final checksum
    auto checksum = calculate_checksum(path);
    if (checksum != file_segment.file_metadata().checksum()) {
        // TODO: cleanup DB entry & remove file
        throw RBException("invalid_checksum");
    }

    auto hash = std::to_string(checksum);
    auto lwt_str = std::to_string(file_segment.file_metadata().last_write_time());
    auto size_str = std::to_string(file_segment.data().size());
    sql = "UPDATE fs SET hash = ?, last_write_time = ?, size = ? WHERE username = ? AND path = ?;";
    
    db.query(sql, {hash, lwt_str, size_str, username, req_c_path});
}

void FileSystemManager::remove_file(const std::string& username, const RBRequest& req) {
    throw RBException("unimplemented:REMOVE");
    //fs::remove(path);
}

std::string FileSystemManager::md5(fs::path path) {
    MD5_CTX md5_ctx;
    MD5_Init(&md5_ctx);

    std::ifstream ifs(path.filename().string(), std::ios::binary);

    char buffer[4096];
    while (ifs.read(buffer, sizeof(buffer)) || ifs.gcount())
        MD5_Update(&md5_ctx, buffer, ifs.gcount());

    unsigned char md[MD5_DIGEST_LENGTH];
    MD5_Final(md, &md5_ctx);

    std::string hash = to_string(md);
    RBLog(std::string("MD5 hash/digest: ") + hash);

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