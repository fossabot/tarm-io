
#include "UTCommon.h"

#include "io/Dir.h"

#include <boost/filesystem.hpp>

#include <sys/stat.h>
//#include <sys/sysmacros.h>

struct DirTest : public testing::Test,
                 public LogRedirector {
    DirTest() {
        m_tmp_test_dir = create_temp_test_directory();
    }

protected:
    boost::filesystem::path m_tmp_test_dir;
};

TEST_F(DirTest, default_constructor) {
    io::EventLoop loop;

    auto dir = new io::Dir(loop);
    EXPECT_TRUE(dir->path().empty());
    EXPECT_FALSE(dir->is_open());

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();
}


TEST_F(DirTest, open_then_close) {
    io::EventLoop loop;
    auto dir = new io::Dir(loop);

    dir->open(m_tmp_test_dir.string(), [&](io::Dir& dir, const io::Status& status) {
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(io::StatusCode::OK, status.code());

        EXPECT_EQ(m_tmp_test_dir, dir.path());
        EXPECT_TRUE(dir.is_open());
        dir.close();
        EXPECT_TRUE(dir.path().empty());
        EXPECT_FALSE(dir.is_open());
    });
    EXPECT_FALSE(dir->is_open());

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();
}

// TODO: open dir which is file

TEST_F(DirTest, open_not_existing) {
    io::EventLoop loop;
    auto dir = new io::Dir(loop);

    bool callback_called = false;

    auto path = (m_tmp_test_dir / "not_exists").string();
    dir->open(path, [&](io::Dir& dir, const io::Status& status) {
        EXPECT_FALSE(status.ok());
        EXPECT_EQ(io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY, status.code());

        callback_called = true;

        // TODO: error check here
        EXPECT_EQ(path, dir.path());
        EXPECT_FALSE(dir.is_open());
    });

    ASSERT_EQ(0, loop.run());

    EXPECT_TRUE(callback_called);
    EXPECT_EQ("", dir->path());
    dir->schedule_removal();
}

TEST_F(DirTest, list_elements) {
    boost::filesystem::create_directories(m_tmp_test_dir/ "dir_1");
    boost::filesystem::create_directories(m_tmp_test_dir/ "dir_2");
    {
        std::ofstream ofile((m_tmp_test_dir/ "file_1").string());
        ASSERT_FALSE(ofile.fail());
    }
    boost::filesystem::create_directories(m_tmp_test_dir/ "dir_3");
    {
        std::ofstream ofile((m_tmp_test_dir/ "file_2").string());
        ASSERT_FALSE(ofile.fail());
    }

    bool dir_1_listed = false;
    bool dir_2_listed = false;
    bool dir_3_listed = false;
    bool file_1_listed = false;
    bool file_2_listed = false;

    io::EventLoop loop;
    auto dir = new io::Dir(loop);
    dir->open(m_tmp_test_dir.string(), [&](io::Dir& dir, const io::Status&) {
        dir.read([&](io::Dir& dir, const char* name, io::DirectoryEntryType entry_type) {
            if (std::string(name) == "dir_1") {
                EXPECT_FALSE(dir_1_listed);
                EXPECT_EQ(io::DirectoryEntryType::DIR, entry_type);
                dir_1_listed = true;
            } else if (std::string(name) == "dir_2") {
                EXPECT_FALSE(dir_2_listed);
                EXPECT_EQ(io::DirectoryEntryType::DIR, entry_type);
                dir_2_listed = true;
            } else if (std::string(name) == "dir_3") {
                EXPECT_FALSE(dir_3_listed);
                EXPECT_EQ(io::DirectoryEntryType::DIR, entry_type);
                dir_3_listed = true;
            } else if (std::string(name) == "file_1") {
                EXPECT_FALSE(file_1_listed);
                EXPECT_EQ(io::DirectoryEntryType::FILE, entry_type);
                file_1_listed = true;
            } else if (std::string(name) == "file_2") {
                EXPECT_FALSE(file_2_listed);
                EXPECT_EQ(io::DirectoryEntryType::FILE, entry_type);
                file_2_listed = true;
            }
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_TRUE(dir_1_listed);
    EXPECT_TRUE(dir_2_listed);
    EXPECT_TRUE(dir_3_listed);
    EXPECT_TRUE(file_1_listed);
    EXPECT_TRUE(file_2_listed);
}

TEST_F(DirTest, close_in_list_callback) {
    // TODO:
}

TEST_F(DirTest, empty_dir) {
    io::EventLoop loop;
    auto dir = new io::Dir(loop);

    // TODO: rename???? to "list"
    bool read_called = false;
    bool end_read_called = false;

    dir->open(m_tmp_test_dir.string(), [&](io::Dir& dir, const io::Status&) {
        dir.read([&](io::Dir& dir, const char* name, io::DirectoryEntryType entry_type) {
            read_called = true;
        }, // end_read
        [&](io::Dir& dir) {
            end_read_called = true;
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_FALSE(read_called);
    EXPECT_TRUE(end_read_called);
}

TEST_F(DirTest, no_read_callback) {
    {
        std::ofstream ofile((m_tmp_test_dir/ "some_file").string());
        ASSERT_FALSE(ofile.fail());
    }

    io::EventLoop loop;
    auto dir = new io::Dir(loop);

    // TODO: rename???? to "list"
    bool end_read_called = false;

    dir->open(m_tmp_test_dir.string(), [&](io::Dir& dir, const io::Status&) {
        dir.read(nullptr,
        [&](io::Dir& dir) { // end_read
            end_read_called = true;
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_TRUE(end_read_called);
}

TEST_F(DirTest, list_symlink) {
    auto file_path = m_tmp_test_dir / "some_file";
    {
        std::ofstream ofile(file_path.string());
        ASSERT_FALSE(ofile.fail());
    }

    boost::filesystem::create_symlink(file_path, m_tmp_test_dir / "link");

    std::size_t entries_count = 0;
    bool link_found = false;

    io::EventLoop loop;
    auto dir = new io::Dir(loop);
    dir->open(m_tmp_test_dir.string(), [&](io::Dir& dir, const io::Status&) {
        dir.read([&](io::Dir& dir, const char* name, io::DirectoryEntryType entry_type) {
            if (std::string(name) == "some_file") {
                EXPECT_EQ(io::DirectoryEntryType::FILE, entry_type);
            } else if (std::string(name) == "link") {
                EXPECT_EQ(io::DirectoryEntryType::LINK, entry_type);
                link_found = true;
            }

            ++entries_count;
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_TRUE(link_found);
    EXPECT_EQ(2, entries_count);
}

#if defined(__APPLE__) || defined(__linux__)
TEST_F(DirTest, list_block_and_char_devices) {
    std::size_t block_devices_count = 0;
    std::size_t char_devices_count = 0;

    io::EventLoop loop;
    auto dir = new io::Dir(loop);
    dir->open("/dev", [&](io::Dir& dir, const io::Status& status) {
        ASSERT_TRUE(status.ok());

        dir.read([&](io::Dir& dir, const char* name, io::DirectoryEntryType entry_type) {
            if (entry_type == io::DirectoryEntryType::BLOCK) {
                ++block_devices_count;
            } else if (entry_type == io::DirectoryEntryType::CHAR) {
                ++char_devices_count;
            }
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_GT(block_devices_count, 0);
    EXPECT_GT(char_devices_count, 0);
}

TEST_F(DirTest, list_domain_sockets) {
    std::size_t domain_sockets_count = 0;

    io::EventLoop loop;
    auto dir = new io::Dir(loop);
    dir->open("/var/run", [&](io::Dir& dir, const io::Status& status) {
        ASSERT_TRUE(status.ok());

        dir.read([&](io::Dir& dir, const char* name, io::DirectoryEntryType entry_type) {
            if (entry_type == io::DirectoryEntryType::SOCKET) {
                ++domain_sockets_count;
            }
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_GT(domain_sockets_count, 0);
}
#endif

// dir iterate not existing
