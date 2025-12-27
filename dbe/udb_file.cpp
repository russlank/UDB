/**
 * @file udb_file.cpp
 * @brief Implementation of the File class for low-level file I/O
 *
 * @author Digixoil
 * @version 2.0.0
 */

#include "udb_file.h"
#include <stdexcept>

namespace udb {

    File::File(const std::string& filename, bool createNew, FileMode mode)
        : m_filename(filename)
    {
        std::ios_base::openmode openMode = std::ios::binary;

        if (hasMode(mode, FileMode::READ)) {
            openMode |= std::ios::in;
        }
        if (hasMode(mode, FileMode::WRITE)) {
            openMode |= std::ios::out;
        }

        if (createNew) {
            // Create new file (truncate if exists)
            m_file.open(filename, openMode | std::ios::trunc);
            if (!m_file.is_open()) {
                // Try creating the file first
                std::ofstream create(filename);
                create.close();
                m_file.open(filename, openMode);
            }
        }
        else {
            m_file.open(filename, openMode);
        }

        if (!m_file.is_open()) {
            m_errorCode = createNew ? ErrorCode::CREATE_ERROR : ErrorCode::OPEN_ERROR;
            throw FileIOException(m_errorCode, "Failed to open file: " + filename);
        }

        m_isOpen = true;
    }

    File::~File()
    {
        close();
    }

    File::File(File&& other) noexcept
        : m_filename(std::move(other.m_filename))
        , m_file(std::move(other.m_file))
        , m_errorCode(other.m_errorCode)
        , m_isOpen(other.m_isOpen)
    {
        other.m_isOpen = false;
    }

    File& File::operator=(File&& other) noexcept
    {
        if (this != &other) {
            close();
            m_filename = std::move(other.m_filename);
            m_file = std::move(other.m_file);
            m_errorCode = other.m_errorCode;
            m_isOpen = other.m_isOpen;
            other.m_isOpen = false;
        }
        return *this;
    }

    void File::close()
    {
        if (m_isOpen) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_file.is_open()) {
                m_file.close();
            }
            m_isOpen = false;
        }
    }

    int64_t File::position() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<int64_t>(m_file.tellg());
    }

    int64_t File::size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Save current position
        auto currentPos = m_file.tellg();

        // Seek to end
        m_file.seekg(0, std::ios::end);
        auto fileSize = m_file.tellg();

        // Restore position
        m_file.seekg(currentPos);

        return static_cast<int64_t>(fileSize);
    }

    int64_t File::seek(int64_t pos, int origin)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::ios_base::seekdir dir;
        switch (origin) {
        case SEEK_SET: dir = std::ios::beg; break;
        case SEEK_CUR: dir = std::ios::cur; break;
        case SEEK_END: dir = std::ios::end; break;
        default: dir = std::ios::beg; break;
        }

        m_file.seekg(pos, dir);
        m_file.seekp(pos, dir);

        if (m_file.fail()) {
            m_file.clear();
            m_errorCode = ErrorCode::SEEK_ERROR;
            throw FileIOException(m_errorCode, "Seek failed");
        }

        return static_cast<int64_t>(m_file.tellg());
    }

    void File::write(const void* buffer, size_t size, int64_t pos)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (pos != INVALID_POSITION) {
            m_file.seekp(pos);
        }

        m_file.write(static_cast<const char*>(buffer), size);

        if (m_file.fail()) {
            m_file.clear();
            m_errorCode = ErrorCode::WRITE_ERROR;
            throw FileIOException(m_errorCode, "Write failed");
        }
    }

    size_t File::read(void* buffer, size_t size, int64_t pos)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (pos != INVALID_POSITION) {
            m_file.seekg(pos);
        }

        m_file.read(static_cast<char*>(buffer), size);

        if (m_file.fail() && !m_file.eof()) {
            m_file.clear();
            m_errorCode = ErrorCode::READ_ERROR;
            throw FileIOException(m_errorCode, "Read failed");
        }

        size_t bytesRead = static_cast<size_t>(m_file.gcount());
        m_file.clear(); // Clear any EOF flags

        return bytesRead;
    }

    void File::flush()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_file.flush();
    }

    bool File::isOpen() const
    {
        return m_isOpen && m_file.is_open();
    }

} // namespace udb
