#include <Arduino.h>
#include "BCH3121.hpp"

const int8_t alpha_to[] = {1, 2, 4, 8, 16, 5, 10, 20, 13, 26,
                           17, 7, 14, 28, 29, 31, 27, 19, 3, 6,
                           12, 24, 21, 15, 30, 25, 23, 11, 22, 9,
                           18, 0};

const int8_t index_of[] = {-1, 0, 1, 18, 2, 5, 19, 11, 3, 29,
                           6, 27, 20, 8, 12, 23, 4, 10, 30, 17,
                           7, 22, 28, 26, 21, 25, 9, 16, 13, 14,
                           24, 15};

#define POCSAG_DATA_MASK   0xFFFFF800;
#define POCSAG_BCH_N       31
#define POCSAG_BCH_K       21

#define POCSAG_BCH_GENPOLY 0x00000769U

CBCH3121::CBCH3121() :
        m_S1(0U),
        m_S3(0U) {
}

void CBCH3121::encode(uint32_t &data) {
    data &= POCSAG_DATA_MASK;

    uint32_t tmp = data;
    uint32_t gen_poly = POCSAG_BCH_GENPOLY << POCSAG_BCH_K;
    for (unsigned int i = 0U; i < POCSAG_BCH_K; i++, tmp <<= 1) {
        if (tmp & 0x80000000U)
            tmp ^= gen_poly;
    }

    data |= (tmp >> POCSAG_BCH_K);
    if (calc_parity(data))
        data |= 0x01U;
}

bool CBCH3121::decode(uint32_t &data, uint16_t &errors, bool &parity) {
    int8_t S1_3, tmp, X1, X2;
    uint8_t cnt = 0U;
    uint8_t Q = 0U;
    uint8_t locator[5U];
    calc_syndrome(data);

    if (m_S1 == -1 && m_S3 == -1) {
        auto var = calc_parity(data);
        if (var)  parity = false;
        errors += var;
        return true;
    } else {
        S1_3 = (m_S1 * 3) % POCSAG_BCH_N;

        if (S1_3 == m_S3) {
            data ^= (0x01U << (m_S1 + 1U));
            auto var = calc_parity(data);
            if (var) parity = false;
            errors += var + 1U;
            return true;
        } else { 
            X1 = m_S1;

            if (m_S3 != -1) tmp = alpha_to[S1_3] ^ alpha_to[m_S3];
            else tmp = alpha_to[S1_3];

            X2 = (index_of[tmp] - m_S1 + POCSAG_BCH_N) % POCSAG_BCH_N;

            for (uint8_t i = 1U; i <= POCSAG_BCH_N; i++) {
                Q = 1U;

                if (X1 != -1) {
                    X1 = (X1 + 1) % POCSAG_BCH_N;
                    Q ^= alpha_to[X1];
                }

                if (X2 != -1) {
                    X2 = (X2 + 2) % POCSAG_BCH_N;
                    Q ^= alpha_to[X2];
                }

                if (!Q) {
                    locator[cnt] = i % POCSAG_BCH_N;
                    cnt++;
                }
            }

            if (cnt != 2U) { 
                errors += check_parity(data) + 3U;
                return false;
            }

            data ^= (0x80000000U >> (locator[0U] - 1U));
            data ^= (0x80000000U >> (locator[1U] - 1U));

            auto var = calc_parity(data);
            if (var) parity = false;
            errors += var + 2U;

            return true;
        }
    }

    return false;
}

void CBCH3121::calc_syndrome(uint32_t data) {
    m_S1 = 0;
    m_S3 = 0;
    data >>= 1;
    for (uint8_t i = 0; i < POCSAG_BCH_N; i++, data >>= 1) {
        if (data & 0x01U) {
            m_S1 ^= alpha_to[i % POCSAG_BCH_N];
            m_S3 ^= alpha_to[(3 * i) % POCSAG_BCH_N];
        }
    }
    m_S1 = index_of[m_S1];
    m_S3 = index_of[m_S3];
}

bool CBCH3121::calc_parity(uint32_t data) {
    data ^= data >> 16;
    data ^= data >> 8;
    data ^= data >> 4;
    data ^= data >> 2;
    data ^= data >> 1;

    return data & 0x01U;
}

uint16_t CBCH3121::check_parity(uint32_t &data) {
    if (calc_parity(data)) {
        data ^= 0x01;
        return 1U;
    } else
        return 0U;
}