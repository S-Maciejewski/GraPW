#!/bin/bash

ipcrm -Q 256
ipcrm -Q 257
ipcrm -Q 258

ipcrm -S 16
ipcrm -S 17
ipcrm -S 18
ipcrm -S 19
ipcrm -S 20
ipcrm -S 21
ipcrm -S 22

ipcrm -M 512
ipcrm -M 513
ipcrm -M 514
ipcrm -M 528
ipcrm -M 529
ipcrm -M 530
ipcrm -M 544
ipcrm -M 545
ipcrm -M 546


exit 0
