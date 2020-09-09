/*
  SIZE   32  33  34  35  36  37  38  39  40  41  42  43  44  45  46  47  48  49  50  51  52  53  54  55  56  57  58  59  60  61  62  63  64  65  66  67  68  69  70  71  72  73  74  75  76  77  78  79  80  81  82  83  84  85  86  87  88  89  90  91  92  93  94  95  96  97  98  99 100 101
  PUT     0   0   0   0   0   0   0   0   1   1   1   1   1   1   1   1   2   2   2   2   2   2   2   2   3   3   3   3   3   3   3   3   4   4   4   4   4   4   4   4   5   5   5   5   5   5   5   5   6   6   6   6   6   6   6   6   7   7   7   7   7   7   7   7   8   8   8   8   8   8
  GET     0   1   1   1   1   1   1   1   1   2   2   2   2   2   2   2   2   3   3   3   3   3   3   3   3   4   4   4   4   4   4   4   4   5   5   5   5   5   5   5   5   6   6   6   6   6   6   6   6   7   7   7   7   7   7   7   7   8   8   8   8   8   8   8   8   9   9   9   9   9

    BSIZE               SIZES
           8          0          8         16         24         32 ...      65512      65520      65528
          32          0         32         64         96        128 ...     262048     262080     262112
        1024          0       1024       2048       3072       4096 ...    8385536    8386560    8387584
        2048          0       2048       4096       6144       8192 ...   16771072   16773120   16775168
        4096          0       4096       8192      12288      16384 ...   33542144   33546240   33550336
        8192          0       8192      16384      24576      32768 ...   67084288   67092480   67100672
       16384          0      16384      32768      49152      65536 ...  134168576  134184960  134201344
      131072          0     131072     262144     393216     524288 ... 1073348608 1073479680 1073610752
*/

#define ROOTS_N 65536

#define ROOTS_GROUP_SIZE 8192

#define ROOTS_GROUPS_REMAINING_0 7ULL
#define ROOTS_GROUPS_REMAINING_1 31ULL
#define ROOTS_GROUPS_REMAINING_2 1023ULL
#define ROOTS_GROUPS_REMAINING_3 2047ULL
#define ROOTS_GROUPS_REMAINING_4 4095ULL
#define ROOTS_GROUPS_REMAINING_5 8191ULL
#define ROOTS_GROUPS_REMAINING_6 16383ULL
#define ROOTS_GROUPS_REMAINING_7 131071ULL

#define ROOTS_MAX_0 65536ULL
#define ROOTS_MAX_1 262144ULL
#define ROOTS_MAX_2 8388608ULL
#define ROOTS_MAX_3 16777216ULL
#define ROOTS_MAX_4 33554432ULL
#define ROOTS_MAX_5 67108864ULL
#define ROOTS_MAX_6 134217728ULL
#define ROOTS_MAX_7 1073741824ULL

#define ROOTS_DIV_0 3
#define ROOTS_DIV_1 5
#define ROOTS_DIV_2 10
#define ROOTS_DIV_3 11
#define ROOTS_DIV_4 12
#define ROOTS_DIV_5 13
#define ROOTS_DIV_6 14
#define ROOTS_DIV_7 17

#define ROOTS_GROUPS_OFFSET_0 0ULL
#define ROOTS_GROUPS_OFFSET_1 8192ULL
#define ROOTS_GROUPS_OFFSET_2 16384ULL
#define ROOTS_GROUPS_OFFSET_3 24576ULL
#define ROOTS_GROUPS_OFFSET_4 32768ULL
#define ROOTS_GROUPS_OFFSET_5 40960ULL
#define ROOTS_GROUPS_OFFSET_6 49152ULL
#define ROOTS_GROUPS_OFFSET_7 57344ULL
