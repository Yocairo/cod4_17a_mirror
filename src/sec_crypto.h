/*
===========================================================================
    Copyright (C) 2010-2013  Ninja and TheKelm of the IceOps-Team

    This file is part of CoD4X17a-Server source code.

    CoD4X17a-Server source code is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    CoD4X17a-Server source code is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>
===========================================================================
*/
#ifndef SEC_CRYPTO_H
#define SEC_CRYPTO_H


//#include "plugin_handler.h"
#include "q_shared.h"
#include "../lib_tomcrypt/main.h"


typedef enum{
    SEC_HASH_SHA1,
    SEC_HASH_SHA256,
    SEC_HASH_TIGER,
    SEC_HASH_SIZE__
}sec_hash_e;

qboolean Sec_HashMemory(sec_hash_e algo, void *in, size_t inSize, void *out, long unsigned int *outSize,qboolean binaryOutput);
qboolean Sec_HashFile(sec_hash_e algo, const char *fname, void *out, long unsigned *outSize,qboolean binaryOutput);
qboolean Sec_BinaryToHex(char *in,unsigned long inSize,char *out,unsigned long *outSize);
extern struct ltc_hash_descriptor sec_hashes[SEC_HASH_SIZE__];

#endif