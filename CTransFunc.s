;
; SFeditor - Star Fighter 3000 map/mission editor
; Transfer function for putting cloud colours into a translation table
; Copyright (C) 2021  Chris Bazley
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public Licence as published by
; the Free Software Foundation; either version 2 of the Licence, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public Licence for more details.
;
; You should have received a copy of the GNU General Public Licence
; along with this program; if not, write to the Free Software
; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
;

  AREA    |C$$code|, CODE, READONLY
  EXPORT  |transfer_func|

; This can't be written in C because it's not APCS-compliant */
;
; On entry:
;   R0 = palette entry (used only to select a cloud colour)
;   R12 = pointer to an array of one PaletteEntry per cloud colour
; On exit:
;   R0 = palette entry for the selected cloud colour

|transfer_func|
  CMP r0,#0
    LDREQ r0,[r12,#0]
    LDRNE r0,[r12,#4]
  MOV pc,lr
  END
