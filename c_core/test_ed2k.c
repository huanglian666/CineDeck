#include "ed2k_calc.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ED2K_CHUNK_SIZE 9728000U
#define DIGEST_SIZE 16U

static void print_hex_inline(const uint8_t digest[16])
{
    for (size_t i = 0; i < DIGEST_SIZE; ++i)
    {
        printf("%02x", digest[i]);
    }
}

static void print_hex_line(const char *label, const uint8_t digest[16])
{
    printf("%s: ", label);
    print_hex_inline(digest);
    printf("\n");
}

static int calc_file_ed2k_demo(const char *file_path)
{
    FILE *fp = fopen(file_path, "rb");
    if (!fp)
    {
        fprintf(stderr, "打开文件失败 [%s]: %s\n", file_path, strerror(errno));
        return -1;
    }

    uint8_t *buf = (uint8_t *)malloc(ED2K_CHUNK_SIZE);
    if (!buf)
    {
        fprintf(stderr, "分配 %u 字节读缓冲失败\n", ED2K_CHUNK_SIZE);
        fclose(fp);
        return -2;
    }

    size_t capacity_chunks = 64;
    size_t chunk_count = 0;
    uint8_t *all_digests = (uint8_t *)malloc(capacity_chunks * DIGEST_SIZE);
    if (!all_digests)
    {
        fprintf(stderr, "分配分块摘要缓冲失败\n");
        free(buf);
        fclose(fp);
        return -3;
    }

    uint64_t total_bytes = 0;
    int rc = 0;

    while (true)
    {
        size_t offset = 0;
        while (offset < ED2K_CHUNK_SIZE)
        {
            size_t n = fread(buf + offset, 1, ED2K_CHUNK_SIZE - offset, fp);
            if (n > 0)
            {
                offset += n;
            }
            else
            {
                if (ferror(fp))
                {
                    fprintf(stderr, "读取文件时发生 I/O 错误\n");
                    rc = -4;
                }
                break;
            }
        }

        if (rc != 0)
        {
            break;
        }

        if (offset == 0 && feof(fp))
        {
            break;
        }

        if (chunk_count == capacity_chunks)
        {
            size_t new_cap = capacity_chunks * 2;
            uint8_t *grown = (uint8_t *)realloc(all_digests, new_cap * DIGEST_SIZE);
            if (!grown)
            {
                fprintf(stderr, "扩容分块摘要缓冲失败\n");
                rc = -5;
                break;
            }
            all_digests = grown;
            capacity_chunks = new_cap;
        }

        uint8_t *chunk_out = all_digests + (chunk_count * DIGEST_SIZE);
        int md4_rc = cinedeck_md4_digest(buf, offset, chunk_out);
        if (md4_rc != 0)
        {
            fprintf(stderr, "第 %zu 块 MD4 计算失败 (rc=%d)\n", chunk_count, md4_rc);
            rc = -6;
            break;
        }

        total_bytes += (uint64_t)offset;
        printf("  [Chunk %zu] bytes=%zu md4=", chunk_count, offset);
        print_hex_inline(chunk_out);
        printf("\n");

        chunk_count += 1;

        if (feof(fp))
        {
            break;
        }
    }

    if (rc == 0)
    {
        if (chunk_count == 0)
        {
            fprintf(stderr, "文件为空（0 字节），跳过 ED2K 计算。\n");
            rc = -7;
        }
        else
        {
            uint8_t ed2k_hash[16] = {0};
            int ed2k_rc = cinedeck_ed2k_hash_from_chunk_digests(all_digests, chunk_count, ed2k_hash);
            if (ed2k_rc != 0)
            {
                fprintf(stderr, "最终 ED2K 计算失败 (rc=%d)\n", ed2k_rc);
                rc = -8;
            }
            else
            {
                printf("\n文件: %s\n", file_path);
                printf("总大小: %" PRIu64 " 字节，分块数: %zu\n", total_bytes, chunk_count);
                print_hex_line("最终 ED2K", ed2k_hash);
            }
        }
    }

    free(all_digests);
    free(buf);
    fclose(fp);
    return rc;
}

int main(int argc, char **argv)
{
    uint8_t digest[16] = {0};

    assert(cinedeck_md4_digest(NULL, 0, digest) == 0);
    print_hex_line("MD4(\"\")", digest);

    const char *abc = "abc";
    assert(cinedeck_md4_digest((const uint8_t *)abc, strlen(abc), digest) == 0);
    print_hex_line("MD4(\"abc\")", digest);

    uint8_t ed2k_single[16] = {0};
    assert(cinedeck_ed2k_hash_from_chunk_digests(digest, 1, ed2k_single) == 0);
    assert(memcmp(digest, ed2k_single, 16) == 0);
    print_hex_line("ED2K(1 chunk)", ed2k_single);

    uint8_t two_chunks[32];
    memcpy(two_chunks, digest, 16);
    memcpy(two_chunks + 16, digest, 16);

    uint8_t ed2k_multi[16] = {0};
    assert(cinedeck_ed2k_hash_from_chunk_digests(two_chunks, 2, ed2k_multi) == 0);
    print_hex_line("ED2K(2 chunks)", ed2k_multi);

    assert(cinedeck_md4_digest(NULL, 5, digest) != 0);
    assert(cinedeck_ed2k_hash_from_chunk_digests(NULL, 1, ed2k_single) != 0);
    assert(cinedeck_ed2k_hash_from_chunk_digests(digest, 0, ed2k_single) != 0);

    printf("基础单元测试通过！\n");

    if (argc >= 2)
    {
        printf("\n--- 开始计算真实文件 ---\n");
        return calc_file_ed2k_demo(argv[1]);
    }
    else
    {
        printf("\n提示：可传入真实文件路径进行测试，例如：\n");
        printf("  %s /path/to/your/sample.mkv\n", argv[0]);
    }

    return 0;
}