#include "FileSystemManager.h"

#include <utility>

bool FileSystemManager::find_file(std::string username, const fs::path& path) {
    if (!fs::exists(path)) {
        RBLog("The path provided doesn't correspond to an existing file");
        return false;
    }

    if (!fs::is_regular_file(path)) {
        RBLog("The path provided doesn't correspond to a file");
        return false;
    }

    auto& db = Database::get_instance();

    std::string parent_path = path.relative_path().string();
    std::string filename = path.filename().string();

    std::string sql = "SELECT COUNT(*) FROM fs WHERE username = ? AND path = ? AND filename = ?;";
    auto count = std::stoi(db.query(sql, {username, parent_path, filename}).at(0));

    if (count == 0) {
        RBLog("The file provided exists but is not present in the db");
        return false;
    }

    return true;
}

bool FileSystemManager::write_file(std::string username, const fs::path& path, const std::string& content,
                                   const std::string& hash, const std::string& last_write_time, const std::string& size) {
    if (path.filename().empty()) {
        RBLog("The path provided doesn't correspond to a file");
        return false;
    }

    auto cpath = fs::weakly_canonical(path);       // Normalize path (even if it doesn't exist)
    fs::create_directories(cpath.parent_path());   // Create directory if it doesn't exist
    const std::string& path_str = cpath.string();  // Get path string
    std::ofstream ofs(path_str);                   // Create file
    if (ofs.is_open()) {
        ofs << content;
        ofs.close();
    } else {
        RBLog("Cannot open file");
        return false;
    }

    /*
    std::uintmax_t file_size = fs::file_size(path);
    if (file_size == static_cast<std::uintmax_t>(-1)) {
        RBLog("Couldn't compute file size");
        return false;
    }
    std::stringstream ss;
    ss << file_size;
    std::string size = ss.str();
    */

    std::string parent_path = path.relative_path().string();
    std::string filename = path.filename().string();

    // Check if the file is already present in db
    auto& db = Database::get_instance();
    std::string sql = "SELECT COUNT(*) FROM fs WHERE username = ? AND path = ? AND filename = ?;";
    auto count = std::stoi(db.query(sql, {username, parent_path, filename}).at(0));

    if (count == 0) {  // Insert entry if file is not already in db
        sql = "INSERT INTO fs (username, path, filename, hash, last_write_time, size) VALUES (?, ?, ?, ?, ?, ?);";
        db.query(sql, {username, parent_path, filename, hash, last_write_time, size});
    } else {  // Update entry if file is already in db
        sql = "UPDATE fs SET hash = ?, last_write_time = ?, size = ? WHERE username = ? AND path = ? AND filename = ?;";
        db.query(sql, {hash, last_write_time, size, username, parent_path, filename});
    }

    return true;
}

bool FileSystemManager::remove_file(std::string username, fs::path path) {
    bool ok = fs::remove(path);
    return ok;
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

    std::string parent_path = path.parent_path().string();
    std::string filename = path.filename().string();
    std::string sql = "SELECT hash FROM fs WHERE username = ? AND path = ? AND filename = ?;";
    auto hash = db.query(sql, {username, parent_path, filename}).at(0);

    return hash;
}

std::string FileSystemManager::get_size(std::string username, const fs::path& path) {
    auto& db = Database::get_instance();

    std::string parent_path = path.parent_path().string();
    std::string filename = path.filename().string();
    std::string sql = "SELECT size FROM fs WHERE username = ? AND path = ? AND filename = ?;";
    auto size = db.query(sql, {username, parent_path, filename}).at(0);

    return size;
}

std::string FileSystemManager::get_last_write_time(std::string username, const fs::path& path) {
    auto& db = Database::get_instance();

    std::string parent_path = path.parent_path().string();
    std::string filename = path.filename().string();
    std::string sql = "SELECT last_write_time FROM fs WHERE username = ? AND path = ? AND filename = ?;";
    auto last_write_time = db.query(sql, {username, parent_path, filename}).at(0);

    return last_write_time;
}