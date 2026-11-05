/**
 * @file FileMetadata.cpp
 * @brief Implementation of FileMetadata binary serialization and deserialization.
 */

#include "FileMetadata.h"
#include <cstring>

namespace da {

void FileMetadata::serialize(std::ostream& os) const {
    os.write(reinterpret_cast<const char*>(&file_id), sizeof(file_id));

    uint32_t name_len = static_cast<uint32_t>(filename.size());
    os.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
    if (name_len > 0) {
        os.write(filename.data(), name_len);
    }

    os.write(reinterpret_cast<const char*>(&file_size), sizeof(file_size));
    
    uint8_t atype = static_cast<uint8_t>(alloc_type);
    os.write(reinterpret_cast<const char*>(&atype), sizeof(atype));

    uint32_t block_count = static_cast<uint32_t>(data_blocks.size());
    os.write(reinterpret_cast<const char*>(&block_count), sizeof(block_count));
    for (int32_t b : data_blocks) {
        os.write(reinterpret_cast<const char*>(&b), sizeof(b));
    }

    os.write(reinterpret_cast<const char*>(&index_block), sizeof(index_block));
    os.write(reinterpret_cast<const char*>(&start_block), sizeof(start_block));

    uint8_t del = deleted ? 1 : 0;
    os.write(reinterpret_cast<const char*>(&del), sizeof(del));

    os.write(reinterpret_cast<const char*>(&created_at), sizeof(created_at));
    os.write(reinterpret_cast<const char*>(&deleted_at), sizeof(deleted_at));
}

FileMetadata FileMetadata::deserialize(std::istream& is) {
    FileMetadata meta;
    is.read(reinterpret_cast<char*>(&meta.file_id), sizeof(meta.file_id));

    uint32_t name_len = 0;
    is.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
    if (name_len > 0 && name_len < 1024) {
        meta.filename.resize(name_len);
        is.read(&meta.filename[0], name_len);
    }

    is.read(reinterpret_cast<char*>(&meta.file_size), sizeof(meta.file_size));

    uint8_t atype = 0;
    is.read(reinterpret_cast<char*>(&atype), sizeof(atype));
    meta.alloc_type = static_cast<AllocationType>(atype);

    uint32_t block_count = 0;
    is.read(reinterpret_cast<char*>(&block_count), sizeof(block_count));
    if (block_count < 10000) {
        meta.data_blocks.resize(block_count);
        for (uint32_t i = 0; i < block_count; ++i) {
            is.read(reinterpret_cast<char*>(&meta.data_blocks[i]), sizeof(int32_t));
        }
    }

    is.read(reinterpret_cast<char*>(&meta.index_block), sizeof(meta.index_block));
    is.read(reinterpret_cast<char*>(&meta.start_block), sizeof(meta.start_block));

    uint8_t del = 0;
    is.read(reinterpret_cast<char*>(&del), sizeof(del));
    meta.deleted = (del != 0);

    is.read(reinterpret_cast<char*>(&meta.created_at), sizeof(meta.created_at));
    is.read(reinterpret_cast<char*>(&meta.deleted_at), sizeof(meta.deleted_at));

    return meta;
}

} // namespace da

