// Host-test stub of pico/types.h. Pico SDK が unsigned int を `uint` で扱うので
// その typedef だけ提供する（実際の Pico では rp2040 ハード型もここに集まっている）。
#pragma once

typedef unsigned int uint;
