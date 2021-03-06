#include "Dir.h"

#include "detail/Common.h"
#include "ScopeExitGuard.h"

//#include <locale>
#include <cstring>
#include <vector>
#include <assert.h>

namespace io {

class Dir::Impl {
public:
    Impl(EventLoop& loop, Dir& parent);
    ~Impl();

    void open(const Path& path, OpenCallback callback);
    bool is_open() const;
    void close();

    void read(ReadCallback read_callback, EndReadCallback end_read_callback = nullptr);

    const Path& path() const;

protected:
    // statics
    static void on_open_dir(uv_fs_t* req);
    static void on_read_dir(uv_fs_t* req);

private:
    static constexpr std::size_t DIRENTS_NUMBER = 1;

    Dir* m_parent = nullptr;
    uv_loop_t* m_uv_loop;

    OpenCallback m_open_callback = nullptr;
    ReadCallback m_read_callback = nullptr;
    EndReadCallback m_end_read_callback = nullptr;

    Path m_path;

    uv_fs_t m_open_dir_req;
    uv_fs_t m_read_dir_req;
    uv_dir_t* m_uv_dir = nullptr;

    uv_dirent_t m_dirents[DIRENTS_NUMBER];
};

namespace {

DirectoryEntryType convert_direntry_type(uv_dirent_type_t type) {
    switch (type) {
        case UV_DIRENT_FILE:
            return DirectoryEntryType::FILE;
        case UV_DIRENT_DIR:
            return DirectoryEntryType::DIR;
        case UV_DIRENT_LINK:
            return DirectoryEntryType::LINK;
        case UV_DIRENT_FIFO:
            return DirectoryEntryType::FIFO;
        case UV_DIRENT_SOCKET:
            return DirectoryEntryType::SOCKET;
        case UV_DIRENT_CHAR:
            return DirectoryEntryType::CHAR;
        case UV_DIRENT_BLOCK:
            return DirectoryEntryType::BLOCK;
        case UV_DIRENT_UNKNOWN:
        default:
            return DirectoryEntryType::UNKNOWN;
    }

    //return DirectoryEntryType::UNKNOWN;
}

}

const Path& Dir::Impl::path() const {
    return m_path;
}

Dir::Impl::Impl(EventLoop& loop, Dir& parent) :
    m_parent(&parent),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {
    memset(&m_open_dir_req, 0, sizeof(m_open_dir_req));
    memset(&m_read_dir_req, 0, sizeof(m_read_dir_req));
}

Dir::Impl::~Impl() {
}

void Dir::Impl::open(const Path& path, OpenCallback callback) {
    m_path = path;
    m_open_callback = callback;
    m_open_dir_req.data = this;

    uv_fs_opendir(m_uv_loop, &m_open_dir_req, path.string().c_str(), on_open_dir);
}

void Dir::Impl::read(ReadCallback read_callback, EndReadCallback end_read_callback) {
    // TODO: check if open
    m_read_dir_req.data = this;
    m_read_callback = read_callback;
    m_end_read_callback = end_read_callback;

    uv_fs_readdir(m_uv_loop, &m_read_dir_req, m_uv_dir, on_read_dir);
}

bool Dir::Impl::is_open() const {
    return m_uv_dir != nullptr;
}

void Dir::Impl::close() {
    if (!is_open()) { // did not open // TODO: revise this for case when open occured with error
        return;
    }

    m_path.clear();

    uv_dir_t* dir = reinterpret_cast<uv_dir_t*>(m_open_dir_req.ptr);

    uv_fs_req_cleanup(&m_open_dir_req);
    uv_fs_req_cleanup(&m_read_dir_req);

    m_open_dir_req.data = nullptr;
    m_read_dir_req.data = nullptr;

    // synchronous
    uv_fs_t closedir_req;
    uv_fs_closedir(m_uv_loop, &closedir_req, dir, nullptr);

    m_uv_dir = nullptr;
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void Dir::Impl::on_open_dir(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<Dir::Impl*>(req->data);

    if (req->result < 0 ) {
        uv_fs_req_cleanup(&this_.m_open_dir_req);

        //std::cerr << "Failed to open dir: " << req->path << std::endl;
        // TODO: error handling
    } else {
        this_.m_uv_dir = reinterpret_cast<uv_dir_t*>(req->ptr);
        this_.m_uv_dir->dirents = this_.m_dirents;
        this_.m_uv_dir->nentries = Dir::Impl::DIRENTS_NUMBER;
    }

    if (this_.m_open_callback) {
        Error error(req->result);
        this_.m_open_callback(*this_.m_parent, error);
    }

    if (req->result < 0 ) { // TODO: replace with if error
        this_.m_path.clear();
    }
}

void Dir::Impl::on_read_dir(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<Dir::Impl*>(req->data);

    uv_dir_t* dir = reinterpret_cast<uv_dir_t*>(req->ptr);

    if (req->result == 0) {
        if (this_.m_end_read_callback) {
            this_.m_end_read_callback(*this_.m_parent);
        }
    } else {
        if (this_.m_read_callback) {
            this_.m_read_callback(*this_.m_parent, this_.m_dirents[0].name, convert_direntry_type(this_.m_dirents[0].type));
        }

        uv_fs_req_cleanup(&this_.m_read_dir_req); // cleaning up previous request
        uv_fs_readdir(req->loop, &this_.m_read_dir_req, dir, on_read_dir);
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////



Dir::Dir(EventLoop& loop) :
    Removable(loop),
    m_impl(new Impl(loop, *this)) {
}

Dir::~Dir() {
}

void Dir::open(const Path& path, OpenCallback callback) {
    return m_impl->open(path, callback);
}

bool Dir::is_open() const {
    return m_impl->is_open();
}

void Dir::close() {
    return m_impl->close();
}

void Dir::read(ReadCallback read_callback, EndReadCallback end_read_callback) {
    return m_impl->read(read_callback, end_read_callback);
}

const Path& Dir::path() const {
    return m_impl->path();
}

void Dir::schedule_removal() {
    m_impl->close();

    Removable::schedule_removal();
}

/////////////////////////////////////////// functions //////////////////////////////////////////////

template<typename CallbackType>
struct RequestWithCallback : public uv_fs_t {
    RequestWithCallback(CallbackType c) :
        callback(c) {
    }

    CallbackType callback = nullptr;
};

void on_make_temp_dir(uv_fs_t* uv_request) {
    auto& request = *reinterpret_cast<RequestWithCallback<MakeTempDirCallback>*>(uv_request);

    if (request.callback) {
        request.callback(request.path, Error(request.result));
    }

    uv_fs_req_cleanup(uv_request);
    delete &request;
}

void make_temp_dir(EventLoop& loop, const Path& name_template, MakeTempDirCallback callback) {
    auto request = new RequestWithCallback<MakeTempDirCallback>(callback);
    const Error error = uv_fs_mkdtemp(reinterpret_cast<uv_loop_t*>(loop.raw_loop()), request, name_template.string().c_str(), on_make_temp_dir);
    if (error) {
        if (callback) {
            callback("", error);
        }

        delete request;
    }
}

void on_make_dir(uv_fs_t* uv_request) {
    auto& request = *reinterpret_cast<RequestWithCallback<MakeDirCallback>*>(uv_request);

    if (request.callback) {
        Error error = request.result;

#ifdef _MSC_VER
        // CreateDirectory function on Windows returns only 2 values on error
        // ERROR_ALREADY_EXISTS and ERROR_PATH_NOT_FOUND
        // to make bahavior consistent between platforms we handle case of long name errors manually
        if (error && strlen(request.path) + 1 > MAX_PATH) { // +1 is for 0 terminating char
            error = Error(StatusCode::NAME_TOO_LONG);
        }
#endif // _MSC_VER

        request.callback(error);
    }

    uv_fs_req_cleanup(uv_request);
    delete &request;
}

void make_dir(EventLoop& loop, const Path& path, MakeDirCallback callback) {
    auto request = new RequestWithCallback<MakeDirCallback>(callback);
    const Error error = uv_fs_mkdir(reinterpret_cast<uv_loop_t*>(loop.raw_loop()), request, path.string().c_str(), 0, on_make_dir);
    if (error) {
        if (callback) {
            callback(error);
        }

        delete request;
    }
}

namespace {

struct RemoveDirWorkEntry {
    RemoveDirWorkEntry(const io::Path& p) :
        path(p),
        processed(false) {
    }

    io::Path path;
    bool processed;
};

struct RemoveDirStatusContext {
    RemoveDirStatusContext(const Error& e, const io::Path& p) :
        error(e),
        path(p) {
    }

    Error error = 0;
    io::Path path;
};

} // namespace

using RemoveDirWorkData = std::vector<RemoveDirWorkEntry>;

RemoveDirStatusContext remove_dir_entry(uv_loop_t* uv_loop, const io::Path& path, Path subpath, RemoveDirWorkData& work_data) {
    work_data.back().processed = true;

    const io::Path open_path = path / subpath;
    uv_fs_t open_dir_req;
    Error open_error = uv_fs_opendir(uv_loop, &open_dir_req, open_path.string().c_str(), nullptr);
    if (open_error) {
        uv_fs_req_cleanup(&open_dir_req);
        return {open_error, open_path};
    }

    uv_dirent_t uv_dir_entry[1];
    auto uv_dir = reinterpret_cast<uv_dir_t*>(open_dir_req.ptr);
    uv_dir->dirents = &uv_dir_entry[0];
    uv_dir->nentries = 1;

    ScopeExitGuard open_req_guard([&open_dir_req, uv_loop, uv_dir](){
        uv_fs_t close_dir_req;
        // Status code of uv_fs_closedir here could be ignored because we guarantee that uv_dir is not nullptr
        uv_fs_closedir(uv_loop, &close_dir_req, uv_dir, nullptr);

        uv_fs_req_cleanup(&close_dir_req);
        uv_fs_req_cleanup(&open_dir_req);
    });

    uv_fs_t read_dir_req;
    int entries_count = 0;

    do {
        ScopeExitGuard read_req_guard([&read_dir_req]() { uv_fs_req_cleanup(&read_dir_req); });

        entries_count = uv_fs_readdir(uv_loop, &read_dir_req, uv_dir, nullptr);
        if (entries_count > 0) {
            auto& entry = uv_dir_entry[0];

            if (entry.type != UV_DIRENT_DIR) {
                uv_fs_t unlink_request;
                const Path unlink_path = path / subpath / entry.name;
                Error unlink_error = uv_fs_unlink(uv_loop, &unlink_request, unlink_path.string().c_str(), nullptr);
                if (unlink_error) {
                    return {unlink_error, unlink_path};
                }
            } else {
                work_data.emplace_back(subpath / entry.name);
            }
        } else if (entries_count < 0) {
            return {entries_count, path};
        }
    } while (entries_count > 0);

    return {Error(0), ""};
}

void remove_dir_impl(EventLoop& loop, const io::Path& path, RemoveDirCallback remove_callback, ProgressCallback progress_callback) {
    loop.add_work([&loop, path, progress_callback]() -> void* {
        auto uv_loop = reinterpret_cast<uv_loop_t*>(loop.raw_loop());

        RemoveDirWorkData work_data;
        work_data.emplace_back(""); // Current directory

        do {
            const auto& status_context = remove_dir_entry(uv_loop, path, work_data.back().path, work_data);
            if (status_context.error) {
                return new RemoveDirStatusContext(status_context);
            }

            auto& last_entry = work_data.back();
            if (last_entry.processed) {
                const Path rmdir_path = path / last_entry.path;
                uv_fs_t rm_dir_req;
                Error rmdir_error = uv_fs_rmdir(uv_loop, &rm_dir_req, rmdir_path.string().c_str(), nullptr);
                if (rmdir_error) {
                    return new RemoveDirStatusContext(rmdir_error, rmdir_path);
                } else {
                    if (progress_callback) {
                        loop.execute_on_loop_thread([progress_callback, rmdir_path](){
                            progress_callback(rmdir_path.string().c_str()); // TODO: remove c_str in future
                        });
                    }
                }

                work_data.pop_back();
            }
        } while(!work_data.empty());

        return new RemoveDirStatusContext(Error(0), "");
    },
    [remove_callback](void* user_data) {
        auto& status_context = *reinterpret_cast<RemoveDirStatusContext*>(user_data);
        remove_callback(status_context.error);
        delete &status_context;
    });
}

void remove_dir(EventLoop& loop, const io::Path& path, RemoveDirCallback remove_callback, ProgressCallback progress_callback) {
    if (loop.is_running()) {
        remove_dir_impl(loop, path, remove_callback, progress_callback);
    } else {
        loop.execute_on_loop_thread([=, &loop](){
            remove_dir_impl(loop, path, remove_callback, progress_callback);
        });
    }
}

} // namespace io
