# ieepview

## Usage

Install on a flashcart of choice.

Note that if you're making use of a menu system, that most likely means the IEEP unlock bit is not set in the header. This will prevent you from editing
bytes at address 0x30 and above.

## Controls

* X1/X2/X3/X4/Y1/Y3: navigate IEEP memory
* A: edit byte under cursor
* Y4: show QR code backup UI
   * X4/X2: change QR codes
   * X1: switch zoom mode (higher zoom = more QR codes)
   * B: exit
* Y2: show SRAM backup/restore UI
