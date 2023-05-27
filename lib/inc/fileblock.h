﻿//
// Created by xSeung on 2023/4/21.
//

#ifndef CN_CD_FILEBLOCK_H
#define CN_CD_FILEBLOCK_H

#include "config.h"
#include "fstream"
#include "string"
#include "vector"

namespace mtft {
    class FileReader : Base {
    public:
        using ptr = std::shared_ptr<FileReader>;
        /// @brief          文件块读取构造函数
        /// @param id       块id
        /// @param fpath    文件地址
        /// @param offset   块偏移
        /// @param length   块长度
        /// @param log      日志器
        FileReader(int id, const std::string& fpath, uint64_t offset, uint64_t length);
        /// @brief 返回是否读取完毕
        /// @return 读取完成:true;否则:false
        [[nodiscard]] bool finished() const;
        /// @brief 读取字节流
        /// @param data
        /// @param buffersize
        /// @return
        uint64_t read(void* data, uint64_t buffersize);
        void     seek(uint64_t progress);
        /// @brief 取得当前块id
        /// @return
        [[nodiscard]] int       getID() const;
        static std::vector<ptr> Builder(int n, uint64_t totalsize, const std::string& pathWithfname);

    private:
        const int     mID;
        std::string   mPathWithFileName;
        std::ifstream mifs;
        uint64_t      mOffset;
        uint64_t      mLength;
        uint64_t      mProgress;
    };
    class FileWriter : Base {
    public:
        using ptr = std::shared_ptr<FileWriter>;
        FileWriter(int id, const std::string& filename, const std::string& path, uint64_t offset, uint64_t length);
        [[nodiscard]] bool      finished() const;
        uint64_t                write(const void* data, uint64_t buffersize);
        void                    seek(uint64_t pos);
        [[nodiscard]] int       getID() const;
        const std::string&      getFname();
        [[nodiscard]] uint64_t  getProgress() const;
        static std::vector<ptr> Builder(int n, uint64_t totalsize, const std::string& filename,
                                        const std::string& path);
        static bool             merge(const std::string& fname, const std::vector<std::string>& vec);
        void                    close();

    private:
        const int                 mid;
        std::string               mFileName;
        std::string               mPath;
        std::ofstream             mofs;
        [[maybe_unused]] uint64_t mOffset;
        uint64_t                  mLength;
        uint64_t                  mProgress;
    };
}  // namespace mtft
#endif  // CN_CD_FILEBLOCK_H
