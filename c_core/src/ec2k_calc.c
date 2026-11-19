#include "ed2k_calc.h"
#include <rhash.h>
#include <stdbool.h>
#include <string.h>

#define CINEDECK_MD4_DIGEST_LEN 16U

static bool g_rhash_initialized = false;

static ensure_rhash_initialized(void)
{
    if (!g_rhash_initialized)
    {
        rhash_library_init();
        g_rhash_initialized = true;
    }
}

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
    uint8_t out_digest[16])
{
    // 1. 判断传入参数是否合法
    if (!out_digest)
    {
        //结果数组不存在
        return -1;
    }

    if (!data && len > 0)
    {
        //数据长度存在，但是指向数据的指针不存在
        return -2;
    }

    // 2. 初始化rhash
    ensure_rhash_initialized();
    rhash ctx = rhash_init(RHASH_MD4);

    if (!ctx)
    {
        //rhash初始化失败
        return -3;
    }

    // 3. 尝试进行MD4计算
    if (len > 0)
    {
        if (rhash_update(ctx, data, len) < 0)
        {
            rhash_free(ctx);
            //MD4计算失败
            return -4;
        }
    }

    // 4. 结果刷入结果集合中
    if (rhash_final(ctx, out_digest) < 0)
    {
        rhash_free(ctx);
        //保存当前块的MD4计算结果失败
        return -5;
    }

    // 5. 回收资源并且返回最终方法执行成功的标志0
    rhash_free(ctx);
    return 0;
}