#include <cstdio>
#include <cstring>

// ============ 工具函数 ============

// 将4位二进制字符串转为十六进制字符
char bt4ToHex(const char* binary) {
    int val = 0;
    for (int i = 0; i < 4; i++) {
        val = val * 2 + (binary[i] - '0');
    }
    if (val < 10) return '0' + val;
    else return 'A' + (val - 10);
}

// 将单个十六进制字符转为数值
int hexCharToVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

// ============ SBOX 定义 ============

const int SBOX[8][4][16] = {
    {{14,4,13,1,2,15,11,8,3,10,6,12,5,9,0,7},
     {0,15,7,4,14,2,13,1,10,6,12,11,9,5,3,8},
     {4,1,14,8,13,6,2,11,15,12,9,7,3,10,5,0},
     {15,12,8,2,4,9,1,7,5,11,3,14,10,0,6,13}},

    {{15,1,8,14,6,11,3,4,9,7,2,13,12,0,5,10},
     {3,13,4,7,15,2,8,14,12,0,1,10,6,9,11,5},
     {0,14,7,11,10,4,13,1,5,8,12,6,9,3,2,15},
     {13,8,10,1,3,15,4,2,11,6,7,12,0,5,14,9}},

    {{10,0,9,14,6,3,15,5,1,13,12,7,11,4,2,8},
     {13,7,0,9,3,4,6,10,2,8,5,14,12,11,15,1},
     {13,6,4,9,8,15,3,0,11,1,2,12,5,10,14,7},
     {1,10,13,0,6,9,8,7,4,15,14,3,11,5,2,12}},

    {{7,13,14,3,0,6,9,10,1,2,8,5,11,12,4,15},
     {13,8,11,5,6,15,0,3,4,7,2,12,1,10,14,9},
     {10,6,9,0,12,11,7,13,15,1,3,14,5,2,8,4},
     {3,15,0,6,10,1,13,8,9,4,5,11,12,7,2,14}},

    {{2,12,4,1,7,10,11,6,8,5,3,15,13,0,14,9},
     {14,11,2,12,4,7,13,1,5,0,15,10,3,9,8,6},
     {4,2,1,11,10,13,7,8,15,9,12,5,6,3,0,14},
     {11,8,12,7,1,14,2,13,6,15,0,9,10,4,5,3}},

    {{12,1,10,15,9,2,6,8,0,13,3,4,14,7,5,11},
     {10,15,4,2,7,12,9,5,6,1,13,14,0,11,3,8},
     {9,14,15,5,2,8,12,3,7,0,4,10,1,13,11,6},
     {4,3,2,12,9,5,15,10,11,14,1,7,6,0,8,13}},

    {{4,11,2,14,15,0,8,13,3,12,9,7,5,10,6,1},
     {13,0,11,7,4,9,1,10,14,3,5,12,2,15,8,6},
     {1,4,11,13,12,3,7,14,10,15,6,8,0,5,9,2},
     {6,11,13,8,1,4,10,7,9,5,0,15,14,2,3,12}},

    {{13,2,8,4,6,15,11,1,10,9,3,14,5,0,12,7},
     {1,15,13,8,10,3,7,4,12,5,6,11,0,14,9,2},
     {7,11,4,1,9,12,14,2,0,6,10,13,15,3,5,8},
     {2,1,14,7,4,10,8,13,15,12,9,0,3,5,6,11}}
};

// ============ 核心函数 ============

void strToBt(const char* s, int len, int bt[64]) {
    for (int i = 0; i < 64; i++) bt[i] = 0;
    
    int realLen = len < 4 ? len : 4;
    
    for (int i = 0; i < realLen; i++) {
        unsigned char k = (unsigned char)s[i];
        for (int j = 0; j < 16; j++) {
            int pow_val = 1;
            for (int m = 15; m > j; m--) pow_val *= 2;
            bt[16*i + j] = (k / pow_val) % 2;
        }
    }
}

void getKeyBytes(const char* key, int keyBytes[][64], int* keyCount) {
    int leng = strlen(key);
    int iterator = leng / 4;
    int remainder = leng % 4;
    *keyCount = iterator + (remainder > 0 ? 1 : 0);
    
    for (int i = 0; i < iterator; i++) {
        strToBt(key + i*4, 4, keyBytes[i]);
    }
    if (remainder > 0) {
        strToBt(key + iterator*4, remainder, keyBytes[iterator]);
    }
}

void bt64ToHex(const int byteData[64], char hex[17]) {
    hex[16] = '\0';
    for (int i = 0; i < 16; i++) {
        char bt[5];
        for (int j = 0; j < 4; j++) {
            bt[j] = '0' + byteData[i*4 + j];
        }
        bt[4] = '\0';
        hex[i] = bt4ToHex(bt);
    }
}

void xorArray(const int a[48], const int b[48], int result[48], int len) {
    for (int i = 0; i < len; i++) {
        result[i] = a[i] ^ b[i];
    }
}

void initPermute(const int originalData[64], int ipByte[64]) {
    int m = 1, n = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 7; j >= 0; j--) {
            int k = 7 - j;
            ipByte[i*8 + k] = originalData[j*8 + m];
            ipByte[i*8 + k + 32] = originalData[j*8 + n];
        }
        m += 2;
        n += 2;
    }
}

void expandPermute(const int rightData[32], int epByte[48]) {
    for (int i = 0; i < 8; i++) {
        if (i == 0) epByte[i*6 + 0] = rightData[31];
        else epByte[i*6 + 0] = rightData[i*4 - 1];
        
        epByte[i*6 + 1] = rightData[i*4 + 0];
        epByte[i*6 + 2] = rightData[i*4 + 1];
        epByte[i*6 + 3] = rightData[i*4 + 2];
        epByte[i*6 + 4] = rightData[i*4 + 3];
        
        if (i == 7) epByte[i*6 + 5] = rightData[0];
        else epByte[i*6 + 5] = rightData[i*4 + 4];
    }
}

void getBoxBinary(int i, char binary[5]) {
    binary[4] = '\0';
    for (int j = 3; j >= 0; j--) {
        binary[j] = '0' + (i & 1);
        i >>= 1;
    }
}

void sBoxPermute(const int expandByte[48], int sBoxByte[32]) {
    for (int m = 0; m < 8; m++) {
        int i = expandByte[m*6 + 0] * 2 + expandByte[m*6 + 5];
        int j = (expandByte[m*6 + 1] << 3) + (expandByte[m*6 + 2] << 2) +
                (expandByte[m*6 + 3] << 1) + expandByte[m*6 + 4];
        
        char binary[5];
        getBoxBinary(SBOX[m][i][j], binary);
        
        sBoxByte[m*4 + 0] = binary[0] - '0';
        sBoxByte[m*4 + 1] = binary[1] - '0';
        sBoxByte[m*4 + 2] = binary[2] - '0';
        sBoxByte[m*4 + 3] = binary[3] - '0';
    }
}

void pPermute(const int sBoxByte[32], int p[32]) {
    const int perm[32] = {
        15,6,19,20,28,11,27,16,
        0,14,22,25,4,17,30,9,
        1,7,23,13,31,26,2,8,
        18,12,29,5,21,10,3,24
    };
    for (int i = 0; i < 32; i++) p[i] = sBoxByte[perm[i]];
}

void finallyPermute(const int endByte[64], int fp[64]) {
    const int perm[64] = {
        39,7,47,15,55,23,63,31,
        38,6,46,14,54,22,62,30,
        37,5,45,13,53,21,61,29,
        36,4,44,12,52,20,60,28,
        35,3,43,11,51,19,59,27,
        34,2,42,10,50,18,58,26,
        33,1,41,9,49,17,57,25,
        32,0,40,8,48,16,56,24
    };
    for (int i = 0; i < 64; i++) fp[i] = endByte[perm[i]];
}

void generateKeys(const int keyByte[64], int keys[16][48]) {
    int key[56];
    const int loop[16] = {1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1};
    
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 8; j++) {
            int k = 7 - j;
            key[i*8 + j] = keyByte[8*k + i];
        }
    }
    
    for (int i = 0; i < 16; i++) {
        for (int l = 0; l < loop[i]; l++) {
            int tempLeft = key[0];
            int tempRight = key[28];
            for (int k = 0; k < 27; k++) {
                key[k] = key[k+1];
                key[28+k] = key[29+k];
            }
            key[27] = tempLeft;
            key[55] = tempRight;
        }
        
        const int tempKey[48] = {
            13,16,10,23,0,4,2,27,
            14,5,20,9,22,18,11,3,
            25,7,15,6,26,19,12,1,
            40,51,30,36,46,54,29,39,
            50,44,32,47,43,48,38,55,
            33,52,45,41,49,35,28,31
        };
        
        for (int x = 0; x < 48; x++) {
            keys[i][x] = key[tempKey[x]];
        }
    }
}

void enc(const int dataByte[64], const int keyByte[64], int result[64]) {
    int keys[16][48];
    generateKeys(keyByte, keys);
    
    int ipByte[64];
    initPermute(dataByte, ipByte);
    
    int ipLeft[32], ipRight[32];
    for (int i = 0; i < 32; i++) {
        ipLeft[i] = ipByte[i];
        ipRight[i] = ipByte[i+32];
    }
    
    for (int i = 0; i < 16; i++) {
        int tempLeft[32];
        for (int j = 0; j < 32; j++) tempLeft[j] = ipLeft[j];
        
        for (int j = 0; j < 32; j++) ipLeft[j] = ipRight[j];
        
        int expanded[48];
        expandPermute(ipRight, expanded);
        
        int xored[48];
        xorArray(expanded, keys[i], xored, 48);
        
        int sBoxOut[32];
        sBoxPermute(xored, sBoxOut);
        
        int pOut[32];
        pPermute(sBoxOut, pOut);
        
        int tempRight[32];
        xorArray(pOut, tempLeft, tempRight, 32);
        
        for (int j = 0; j < 32; j++) ipRight[j] = tempRight[j];
    }
    
    int finalData[64];
    for (int i = 0; i < 32; i++) {
        finalData[i] = ipRight[i];
        finalData[i+32] = ipLeft[i];
    }
    
    finallyPermute(finalData, result);
}

// ============ 主加密函数 ============

void strEnc(const char* data, const char* firstKey, const char* secondKey, 
            const char* thirdKey, char* encData) {
    encData[0] = '\0';
    
    int firstKeyBt[16][64], secondKeyBt[16][64], thirdKeyBt[16][64];
    int firstKeyCnt, secondKeyCnt, thirdKeyCnt;
    
    getKeyBytes(firstKey, firstKeyBt, &firstKeyCnt);
    getKeyBytes(secondKey, secondKeyBt, &secondKeyCnt);
    getKeyBytes(thirdKey, thirdKeyBt, &thirdKeyCnt);
    
    int leng = strlen(data);
    int iterator = leng / 4;
    int remainder = leng % 4;
    
    char tempHex[17];
    
    for (int i = 0; i < iterator; i++) {
        int tempBt[64];
        strToBt(data + i*4, 4, tempBt);
        
        for (int x = 0; x < firstKeyCnt; x++) {
            int result[64];
            enc(tempBt, firstKeyBt[x], result);
            for (int j = 0; j < 64; j++) tempBt[j] = result[j];
        }
        for (int y = 0; y < secondKeyCnt; y++) {
            int result[64];
            enc(tempBt, secondKeyBt[y], result);
            for (int j = 0; j < 64; j++) tempBt[j] = result[j];
        }
        for (int z = 0; z < thirdKeyCnt; z++) {
            int result[64];
            enc(tempBt, thirdKeyBt[z], result);
            for (int j = 0; j < 64; j++) tempBt[j] = result[j];
        }
        
        bt64ToHex(tempBt, tempHex);
        strcat(encData, tempHex);
    }
    
    if (remainder > 0) {
        int tempBt[64];
        strToBt(data + iterator*4, remainder, tempBt);
        
        for (int x = 0; x < firstKeyCnt; x++) {
            int result[64];
            enc(tempBt, firstKeyBt[x], result);
            for (int j = 0; j < 64; j++) tempBt[j] = result[j];
        }
        for (int y = 0; y < secondKeyCnt; y++) {
            int result[64];
            enc(tempBt, secondKeyBt[y], result);
            for (int j = 0; j < 64; j++) tempBt[j] = result[j];
        }
        for (int z = 0; z < thirdKeyCnt; z++) {
            int result[64];
            enc(tempBt, thirdKeyBt[z], result);
            for (int j = 0; j < 64; j++) tempBt[j] = result[j];
        }
        
        bt64ToHex(tempBt, tempHex);
        strcat(encData, tempHex);
    }
}