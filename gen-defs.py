#!/usr/bin/python

### O HUGEPAGE é PROCESSES[(offset, size),] |PROCESS0|PROCESS2|
####  TODO: FIXME: ja executar ele rodando como root, scheduling e cpu

ROOTS_N = 4096   + 16

N_START = 5

# PARA FICAR UNIFORME TEM QUE SER X_DIVISOR = (X_COUNT*X_SALT)
X_SALT = 1
X_DIVISOR = 128 # (ISSO - 1) -> QUANTOS FICAM NO MEIO ENTRE A ... 2*A

# TODO: FIXME: IMPLEMENTAR NO CÓDIGO; PODE SER POSITIVO OU NEGATIVO
INITIAL = 0
# 0k 924k 928k 932k 936k 940k 944k 948k 952k 956k 960k 964k 968k 972k 976k 980k 984k 988k 992k 996k 1000k 1004k 1008k 1012k 1016k 1020k 1024k
def sequence():

    idx = 0

    n = N_START # n é a potencia

    X_DIVISOR=4
    while True:
        x = 0
        # X_DIVISOR é o quanto ele vai ficar quebrando uma potência
        while x != X_DIVISOR:
            # yield INITIAL + (1 << n) + ((x * (1 << n)) // X_DIVISOR)
            yield n
            idx += 1
            if idx == ROOTS_N:
                return
            x += X_SALT
        X_DIVISOR += X_DIVISOR//2
        n += 1
#                   ((idx//ROOTS_N)+1)
SEQUENCE = tuple(sequence())

assert len(SEQUENCE) == ROOTS_N

ROOTS_SIZES_0, *_, ROOTS_SIZES_N = SEQUENCE

C_SIZE_MIN = ROOTS_SIZES_0
# É O ROOTS_SIZES_N, ARREDONDADO PARA BAIXO
C_SIZE_MAX = ROOTS_SIZES_N - (ROOTS_SIZES_N % 8)
# 32 36 40 44 48 52 56 60 64 72 80 88 96 104 112 120 128 144 160 176 192 208 224 240 256 288 320 352 384 416 448 480 512 576 640 704 768 832 896 960 1024 1k 1k 1k 1k 1k 1k 1k 2k 2k 2k 2k 3k 3k 3k 3k 4k 4k 5k 5k 6k 6k 7k 7k 8k 9k 10k 11k 12k 13k 14k 15k 16k 18k 20k 22k 24k 26k 28k 30k 32k 36k 40k 44k 48k 52k 56k 60k 64k 72k 80k 88k 96k 104k 112k 120k 128k 144k 160k 176k 192k 208k 224k 240k 256k 288k 320k 352k 384k 416k 448k 480k 512k 576k 640k 704k 768k 832k 896k 960k 1024k 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 1g 1g 1g 1g 1g 1g 1g 1g 2g 2g 2g 2g 3g 3g 3g 3g 4g 4g 5g 5g 6g 6g 7g 7g 8g 9g 10g 11g 12g 13g 14g 15g 16g 18g 20g 22g 24g 26g 28g 30g 32g 36g 40g 44g 48g 52g 56g 60g 64g 72g 80g 88g 96g 104g 112g 120g
# 32 40 48 56 64 72 80 88 96 112 128 144 160 176 192 208 224 256 288 320 352 384 416 448 480 544 608 672 736 800 864 928 992 1k 1k 1k 1k 1k 1k 1k 1k 2k 2k 2k 2k 3k 3k 3k 3k 4k 4k 5k 5k 6k 6k 7k 7k 8k 9k 10k 11k 12k 13k 14k 15k 17k 19k 21k 23k 25k 27k 29k 31k 35k 39k 43k 47k 51k 55k 59k 63k 71k 79k 87k 95k 103k 111k 119k 127k 143k 159k 175k 191k 207k 223k 239k 255k 287k 319k 351k 383k 415k 447k 479k 511k 575k 639k 703k 767k 831k 895k 959k 1023k 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 0g 1g 1g 1g 1g 1g 1g 1g 1g 2g 2g 2g 2g 3g 3g 3g 3g 4g 4g 5g 5g 6g 6g 7g 7g 8g 9g 10g 11g 12g 13g 14g 15g 17g 19g 21g 23g 25g 27g 29g 31g 35g 39g 43g 47g 51g 55g 59g 63g 71g 79g 87g 95g 103g 111g 119g 127g 143g 159g 175g 191g 207g 223g 239g

C_SIZE_MIN_ = '%016X' % C_SIZE_MIN
C_SIZE_MAX_ = '%016X' % C_SIZE_MAX

FST_ = '%016X' % ROOTS_SIZES_0
LST_ = '%016X' % ROOTS_SIZES_N

OPEN, CLOSE = '{', '}'

ROOTS_SIZES = '{' + ','.join(('0x%XULL' % s for s in SEQUENCE)) + '}'

SUFFIX = 'ULL'

print(f'''
#define ROOTS_N {ROOTS_N}

#define ROOTS_SIZES_0 0x{FST_}{SUFFIX} // {ROOTS_SIZES_0}
#define ROOTS_SIZES_N 0x{LST_}{SUFFIX} // {ROOTS_SIZES_N}

static const u64 ROOTS_SIZES[] = {ROOTS_SIZES};

#define N_START {N_START}

#define X_DIVISOR {X_DIVISOR}
#define X_SALT {X_SALT}
#define X_DIVISOR {X_DIVISOR}

#define C_SIZE_MIN 0x{C_SIZE_MIN_}{SUFFIX} // {C_SIZE_MIN}
#define C_SIZE_MAX 0x{C_SIZE_MAX_}{SUFFIX} // {C_SIZE_MAX}
''')

### SOMENTE PARA VISUALIZAR
print(' '.join((((f'{x//(1024*1024*1024)}g' if x > 1024*1024 else f'{x//(1024*1024)}m') if x > 1024*1024 else f'{x//1024}k') if x > 1024 else f'{x}') for x in SEQUENCE))

# É DESPERDÍCIO PROVIDENCIAR TAMANHOS MENORES DO QUE O QUE VAI SER ALOCADO
# O MÁXIMO TEM DE SER GARANTIDO
assert C_SIZE_MIN <= ROOTS_SIZES_0 < C_SIZE_MAX <= ROOTS_SIZES_N <= (1 << 63)

print(SEQUENCE)


# roots pequenos representam o proximo livre

# os pequenos representam
