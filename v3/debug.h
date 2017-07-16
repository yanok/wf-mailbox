#ifndef _DEBUG_H_
#define _DEBUG_H_
#include <stdio.h>

#ifdef DEBUG
#define debug(format, ...) printf("%s:%d:" format, __func__, __LINE__, __VA_ARGS__)
#else
#define debug(...) do {} while (0)
#endif /* DEBUG */

#endif /* _DEBUG_H_ */
