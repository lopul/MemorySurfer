
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
//  Description:
//      This file implements the Secure Hashing Algorithm 1 as
//      defined in FIPS PUB 180-1 published April 17, 1995.
//
//      The SHA-1, produces a 160-bit message digest for a given
//      data stream.  It should take about 2**n steps to find a
//      message with the same digest as a given message and
//      2**(n/2) to find any two messages with the same digest,
//      when n is the digest size in bits.  Therefore, this
//      algorithm can serve as a means of providing a
//      "fingerprint" for a message.
//
//  Portability Issues:
//      SHA-1 is defined in terms of 32-bit "words".  This code
//      uses <stdint.h> (included via "sha1.h" to define 32 and 8
//      bit unsigned integer types.  If your C compiler does not
//      support 32 bit unsigned integers, this code is not
//      appropriate.
//
//  Caveats:
//      SHA-1 is designed to work with messages less than 2^64 bits
//      long.  Although SHA-1 allows a message digest to be generated
//      for messages of any number of bits less than 2^64, this
//      implementation only works with messages with a length that is
//      a multiple of the size of an 8-bit character.
//

#include "sha1.h"
#include <malloc.h>
#include <assert.h>

// Define the SHA1 circular left shift macro
#define SHA1CircularShift(bits,word) (((word) << (bits)) | ((word) >> (32-(bits))))

//
//  Description:
//      This function will initialize the Sha1Context in preparation
//      for computing a new SHA1 message digest.
//
//  Parameters:
//      context: [in/out]
//          The context to reset.
//
int sha1_reset (struct Sha1Context *context)
{
  int err;
  err = context == NULL;
  if (err == 0)
  {
    context->Length_Low             = 0;
    context->Length_High            = 0;
    context->Message_Block_Index    = 0;

    context->Intermediate_Hash[0]   = 0x67452301;
    context->Intermediate_Hash[1]   = 0xEFCDAB89;
    context->Intermediate_Hash[2]   = 0x98BADCFE;
    context->Intermediate_Hash[3]   = 0x10325476;
    context->Intermediate_Hash[4]   = 0xC3D2E1F0;
  }
  return err;
}

//
//  Description:
//      This function will process the next 512 bits of the message
//      stored in the message_block array.
//
//  Comments:
//      Many of the variable names in this code, especially the
//      single character names, were used because those were the
//      names used in the publication.
//
void SHA1ProcessMessageBlock (struct Sha1Context *context)
{
  const uint32_t K[] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 }; // Constants defined in SHA-1
  int           t;                 // Loop counter
  uint32_t      temp;              // Temporary word value
  uint32_t      W[80];             // Word sequence
  uint32_t      A, B, C, D, E;     // Word buffers

  uint32_t W_t; // temp

  //
  //  Initialize the first 16 words in the array W
  //
  for (t = 0; t < 16; t++)
  {
    W[t] = context->message_block[t * 4] << 24;
    W[t] |= context->message_block[t * 4 + 1] << 16;
    W[t] |= context->message_block[t * 4 + 2] << 8;
    W[t] |= context->message_block[t * 4 + 3];
  }

  for (t = 16; t < 80; t++)
  {
    W_t = W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16];
    W[t] = SHA1CircularShift(1,W_t);
  }

  A = context->Intermediate_Hash[0];
  B = context->Intermediate_Hash[1];
  C = context->Intermediate_Hash[2];
  D = context->Intermediate_Hash[3];
  E = context->Intermediate_Hash[4];

  for (t = 0; t < 20; t++)
  {
    temp = SHA1CircularShift(5,A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }

  for (t = 20; t < 40; t++)
  {
    temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }

  for (t = 40; t < 60; t++)
  {
    temp = SHA1CircularShift(5,A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }

  for (t = 60; t < 80; t++)
  {
    temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }

  context->Intermediate_Hash[0] += A;
  context->Intermediate_Hash[1] += B;
  context->Intermediate_Hash[2] += C;
  context->Intermediate_Hash[3] += D;
  context->Intermediate_Hash[4] += E;

  context->Message_Block_Index = 0;
}

//
//  Description:
//      This function accepts an array of octets as the next portion
//      of the message.
//
//  Parameters:
//      context: [in/out]
//          The SHA context to update
//      message_array: [in]
//          An array of characters representing the next portion of
//          the message.
//      length: [in]
//          The length of the message in message_array
//
int sha1_input (struct Sha1Context *context, const uint8_t *message_array, unsigned int length)
{
  int err;
  err = 0;
  if (length > 0)
  {
    err = (context == NULL) || (message_array == NULL);
    while (length-- && err == 0)
    {
      context->message_block[context->Message_Block_Index++] = *message_array;
      assert ((*message_array) == (*message_array & 0xFF));
      context->Length_Low += 8;
      if (context->Length_Low == 0)
      {
        context->Length_High++;
        err = context->Length_High == 0; // Message is too long
      }
      if (context->Message_Block_Index == 64)
      {
        SHA1ProcessMessageBlock (context);
      }
      message_array++;
    }
  }
  return err;
}

//
// Description:
//   According to the standard, the message must be padded to an even
//   512 bits.  The first padding bit must be a '1'.  The last 64
//   bits represent the length of the original message.  All bits in
//   between should be 0.  This function will pad the message
//   according to those rules by filling the message_block array
//   accordingly.  It will also call the ProcessMessageBlock function
//   provided appropriately.  When it returns, it can be assumed that
//   the message digest has been computed.
//
// Parameters:
//   context: [in/out]
//     The context to pad
//   ProcessMessageBlock: [in]
//     The appropriate SHA*ProcessMessageBlock function
//
void SHA1PadMessage (struct Sha1Context *context)
{
  //
  // Check to see if the current message block is too small to hold
  // the initial padding bits and length.  If so, we will pad the
  // block, process it, and then continue padding into a second
  // block.
  //
  if (context->Message_Block_Index > 55)
  {
    context->message_block[context->Message_Block_Index++] = 0x80;
    while(context->Message_Block_Index < 64)
    {
      context->message_block[context->Message_Block_Index++] = 0;
    }

    SHA1ProcessMessageBlock(context);

    while(context->Message_Block_Index < 56)
    {
      context->message_block[context->Message_Block_Index++] = 0;
    }
  }
  else
  {
    context->message_block[context->Message_Block_Index++] = 0x80;
    while(context->Message_Block_Index < 56)
    {
      context->message_block[context->Message_Block_Index++] = 0;
    }
  }

  //
  //  Store the message length as the last 8 octets
  //
  context->message_block[56] = context->Length_High >> 24;
  context->message_block[57] = context->Length_High >> 16;
  context->message_block[58] = context->Length_High >> 8;
  context->message_block[59] = context->Length_High;
  context->message_block[60] = context->Length_Low >> 24;
  context->message_block[61] = context->Length_Low >> 16;
  context->message_block[62] = context->Length_Low >> 8;
  context->message_block[63] = context->Length_Low;

  SHA1ProcessMessageBlock(context);
}

//
//  Description:
//      This function will return the 160-bit message digest into the
//      message_digest array  provided by the caller.
//      NOTE: The first octet of hash is stored in the 0th element,
//            the last octet of hash in the 19th element.
//
//  Parameters:
//      context: [in/out]
//          The context to use to calculate the SHA-1 hash.
//      message_digest: [out]
//          Where the digest is returned.
//
int sha1_result (struct Sha1Context *context, uint8_t message_digest[SHA1_HASH_SIZE])
{
  int err;
  int i;
  err = (context == NULL) || (message_digest == NULL);
  if (err == 0)
  {
    SHA1PadMessage (context);
  }

  for (i=0; i<64; ++i)
  {
    // message may be sensitive, clear it out
    context->message_block[i] = 0;
  }
  context->Length_Low = 0;  // and clear length
  context->Length_High = 0;

  for (i = 0; i < SHA1_HASH_SIZE; ++i)
  {
    message_digest[i] = context->Intermediate_Hash[i>>2] >> 8 * ( 3 - ( i & 0x03 ) );
  }

  return err;
}
