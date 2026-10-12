#if !defined(BCH3121_H)
#define  BCH3121_H

class CBCH3121 {
public:
    CBCH3121();

    void encode(uint32_t &data);

    bool decode(uint32_t &data, uint16_t &errors, bool &parity);

private:
    void calc_syndrome(uint32_t data);

    static bool calc_parity(uint32_t data);

    static uint16_t check_parity(uint32_t &data);

    int8_t m_S1;
    int8_t m_S3;

};

#endif