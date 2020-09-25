
//
// Author: Lorenz Pullwitt <memorysurfer@lorenz-pullwitt.de>
// Copyright 2017-2020
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, you can find it here:
// https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
//

//
// Description:
//  This is the header file for code which implements the Secure
//  Hashing Algorithm 1 as defined in FIPS PUB 180-1 published
//  April 17, 1995.
//
//  Many of the variable names in this code, especially the
//  single character names, were used because those were the names
//  used in the publication.
//
//  Please read the file sha1.c for more information.
//

#include <stdint.h>

enum { SHA1_HASH_SIZE = 20 };

//
// This structure will hold context information for the SHA-1 hashing operation
//
struct Sha1Context
{
  uint32_t Intermediate_Hash[SHA1_HASH_SIZE/4]; // Message Digest

  uint32_t Length_Low;  // Message length in bits
  uint32_t Length_High; // Message length in bits

  int_least16_t Message_Block_Index; // Index into message block array
  uint8_t message_block[64];         // 512-bit message blocks
};

int sha1_reset (struct Sha1Context *context);
int sha1_input (struct Sha1Context *context, const uint8_t *message_array, unsigned int length);
int sha1_result (struct Sha1Context *context, uint8_t message_digest[SHA1_HASH_SIZE]);
