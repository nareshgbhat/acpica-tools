/*
 * Some or all of this work - Copyright (c) 2006 - 2013, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of Intel Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Bug 0010:
 *
 * SUMMARY: ToBuffer transforms operand (in local variable) to reference
 */

Method(mda9)
{
	Store(Buffer(4){10, 11, 12, 13}, Local0)

	ToBuffer(Local0, Local1)

	Store(ObjectType(Local1), Local2)
	if (LNotequal(Local2, c00b)) {
		err("", zFFF, 0x000, 0, 0, Local2, c00b)
	}
	if (LNotequal(Local1, Buffer(4){10, 11, 12, 13})) {
		err("", zFFF, 0x001, 0, 0, Local1, Buffer(4){10, 11, 12, 13})
	}

	Store(ObjectType(Local0), Local2)
	if (LNotequal(Local2, c00b)) {
		err("", zFFF, 0x002, 0, 0, Local2, c00b)
	}
	if (LNotequal(Local0, Buffer(4){10, 11, 12, 13})) {
		err("", zFFF, 0x003, 0, 0, Local0, Buffer(4){10, 11, 12, 13})
	}
}