#include <cstdint>
#include <cstring>

static constexpr int KECCAK_RATE = 136;

static const uint64_t RC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808AULL,
    0x8000000080008000ULL, 0x000000000000808BULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008AULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000AULL,
    0x000000008000808BULL, 0x800000000000008BULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800AULL, 0x800000008000000AULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,
};

static inline uint64_t rotl64(uint64_t x, int n) {
    return (x << n) | (x >> (64 - n));
}

static void keccak_f1600_host(uint64_t A[25]) {
    uint64_t C[5], D[5], t;
    for (int round = 0; round < 24; ++round) {
        C[0]=A[0]^A[5]^A[10]^A[15]^A[20]; C[1]=A[1]^A[6]^A[11]^A[16]^A[21];
        C[2]=A[2]^A[7]^A[12]^A[17]^A[22]; C[3]=A[3]^A[8]^A[13]^A[18]^A[23];
        C[4]=A[4]^A[9]^A[14]^A[19]^A[24];
        D[0]=C[4]^rotl64(C[1],1); D[1]=C[0]^rotl64(C[2],1);
        D[2]=C[1]^rotl64(C[3],1); D[3]=C[2]^rotl64(C[4],1);
        D[4]=C[3]^rotl64(C[0],1);
        A[0]^=D[0];A[5]^=D[0];A[10]^=D[0];A[15]^=D[0];A[20]^=D[0];
        A[1]^=D[1];A[6]^=D[1];A[11]^=D[1];A[16]^=D[1];A[21]^=D[1];
        A[2]^=D[2];A[7]^=D[2];A[12]^=D[2];A[17]^=D[2];A[22]^=D[2];
        A[3]^=D[3];A[8]^=D[3];A[13]^=D[3];A[18]^=D[3];A[23]^=D[3];
        A[4]^=D[4];A[9]^=D[4];A[14]^=D[4];A[19]^=D[4];A[24]^=D[4];
        t=A[1];
        A[1]=rotl64(A[6],44);A[6]=rotl64(A[9],20);A[9]=rotl64(A[22],61);
        A[22]=rotl64(A[14],39);A[14]=rotl64(A[20],18);A[20]=rotl64(A[2],62);
        A[2]=rotl64(A[12],43);A[12]=rotl64(A[13],25);A[13]=rotl64(A[19],8);
        A[19]=rotl64(A[23],56);A[23]=rotl64(A[15],41);A[15]=rotl64(A[4],27);
        A[4]=rotl64(A[24],14);A[24]=rotl64(A[21],2);A[21]=rotl64(A[8],55);
        A[8]=rotl64(A[16],45);A[16]=rotl64(A[5],36);A[5]=rotl64(A[3],28);
        A[3]=rotl64(A[18],21);A[18]=rotl64(A[17],15);A[17]=rotl64(A[11],10);
        A[11]=rotl64(A[7],6);A[7]=rotl64(A[10],3);A[10]=rotl64(t,1);
        for(int i=0;i<25;i+=5){
            uint64_t a0=A[i],a1=A[i+1],a2=A[i+2],a3=A[i+3],a4=A[i+4];
            A[i]=a0^((~a1)&a2);A[i+1]=a1^((~a2)&a3);
            A[i+2]=a2^((~a3)&a4);A[i+3]=a3^((~a4)&a0);A[i+4]=a4^((~a0)&a1);
        }
        A[0]^=RC[round];
    }
}

extern "C" void keccak256_host(const uint8_t* input, size_t len, uint8_t output[32]) {
    uint64_t A[25];
    std::memset(A, 0, sizeof(A));
    size_t offset = 0;
    while (offset + KECCAK_RATE <= len) {
        for (int i = 0; i < KECCAK_RATE/8; ++i) {
            uint64_t lane; std::memcpy(&lane, input+offset+i*8, 8);
            A[i] ^= lane;
        }
        keccak_f1600_host(A);
        offset += KECCAK_RATE;
    }
    uint8_t lastBlock[KECCAK_RATE];
    std::memset(lastBlock, 0, KECCAK_RATE);
    size_t remaining = len - offset;
    std::memcpy(lastBlock, input + offset, remaining);
    lastBlock[remaining] = 0x01;
    lastBlock[KECCAK_RATE - 1] |= 0x80;
    for (int i = 0; i < KECCAK_RATE/8; ++i) {
        uint64_t lane; std::memcpy(&lane, lastBlock+i*8, 8);
        A[i] ^= lane;
    }
    keccak_f1600_host(A);
    std::memcpy(output, A, 32);
}
