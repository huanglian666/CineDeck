#ifndef CINEDECK_ED2K_CALC_H
#define CINEDECK_ED2K_CALC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 计算单段字节缓冲区 [0..len) 的 MD4 摘要（16 字节）。
 *
 * - 当 len == 0 时，data 允许为 NULL（空输入）。
 * - out_digest 必须指向至少 16 字节的可写内存。
 * 
 * @param data 一个不可变的指向数据起始地址的指针
 * @param len 需要读取的数据长度
 * @param out_digest 存放每一段计算结果的数组
 *
 * @return 0 表示成功；非 0 表示参数无效或底层计算失败。
 */
int cinedeck_md4_digest(
    const uint8_t *data,
    size_t len,
    uint8_t out_digest[16]
);

/**
 * @brief 根据各分块的 MD4 摘要列表计算最终 ED2K 哈希（16 字节）。
 *
 * - chunk_digests_concat 为连续存放的 chunk_count * 16 字节数据。
 * - chunk_count == 1 时直接拷贝该分块的 16 字节摘要；
 *   chunk_count > 1 时将所有分块摘要按顺序拼接后再计算一次 MD4。
 * - out_digest 必须指向至少 16 字节的可写内存。
 *
 * @param chunk_digests_concat 连续存放各分块 16 字节 MD4 的只读指针
 * @param chunk_count 分块数量（必须 > 0）
 * @param out_digest 存放最终 16 字节 ED2K 哈希的输出缓冲区
 *
 * @return 0 表示成功；非 0 表示参数无效或底层计算失败。
 */
int cinedeck_ed2k_hash_from_chunk_digests(
    const uint8_t *chunk_digests_concat,
    size_t chunk_count,
    uint8_t out_digest[16]
);

#ifdef __cplusplus
}
#endif
#endif /* CINEDECK_ED2K_CALC_H */