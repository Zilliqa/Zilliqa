# Copyright (c) 2017 Zilliqa 
# This source code is being disclosed to you solely for the purpose of your participation in 
# testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
# the protocols and algorithms that are programmed into, and intended by, the code. You may 
# not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
# including modifying or publishing the code (or any part of it), and developing or forming 
# another public or private blockchain network. This source code is provided ‘as is’ and no 
# warranties are given as to title or non-infringement, merchantability or fitness for purpose 
# and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
# Some programs in this code are governed by the GNU General Public License v3.0 (available at 
# https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
# GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
# and which include a reference to GPLv3 in their program files.

python tests/Zilliqa/test_zilliqa_local.py stop
python tests/Zilliqa/test_zilliqa_local.py clean
python tests/Zilliqa/test_zilliqa_local.py setup 8
python tests/Zilliqa/test_zilliqa_local.py start
python tests/Zilliqa/test_zilliqa_local.py connect
python tests/Zilliqa/test_zilliqa_local.py sendcmd 1 0104
python tests/Zilliqa/test_zilliqa_local.py sendcmd 4 010000000004fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc1111111111111111111111111111111111111111111111111111111111111114
python tests/Zilliqa/test_zilliqa_local.py sendcmd 2 010000000001ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff1111111111111111111111111111111111111111111111111111111111111111
python tests/Zilliqa/test_zilliqa_local.py sendcmd 1 010000000002fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe1111111111111111111111111111111111111111111111111111111111111112
python tests/Zilliqa/test_zilliqa_local.py sendcmd 3 010000000003fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffd1111111111111111111111111111111111111111111111111111111111111113
sleep 15
python tests/Zilliqa/test_zilliqa_local.py sendcmd 5 010100000010ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffed1111111111111111111111111111111111111111111111111111111111111120
python tests/Zilliqa/test_zilliqa_local.py sendcmd 1 010100000011ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffea1111111111111111111111111111111111111111111111111111111111111121
python tests/Zilliqa/test_zilliqa_local.py sendcmd 3 010100000012ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffef1111111111111111111111111111111111111111111111111111111111111122
python tests/Zilliqa/test_zilliqa_local.py sendcmd 2 010100000013ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe11111111111111111111111111111111111111111111111111111111111111123
python tests/Zilliqa/test_zilliqa_local.py sendcmd 8 010100000014ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe51111111111111111111111111111111111111111111111111111111111111124
python tests/Zilliqa/test_zilliqa_local.py sendcmd 2 010100000015ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe41111111111111111111111111111111111111111111111111111111111111125
python tests/Zilliqa/test_zilliqa_local.py sendcmd 6 010100000016ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe21111111111111111111111111111111111111111111111111111111111111126
python tests/Zilliqa/test_zilliqa_local.py sendcmd 4 010100000017ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe31111111111111111111111111111111111111111111111111111111111111127
python tests/Zilliqa/test_zilliqa_local.py sendcmd 7 010100000018ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeb1111111111111111111111111111111111111111111111111111111111111128
python tests/Zilliqa/test_zilliqa_local.py sendcmd 7 010100000019ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe61111111111111111111111111111111111111111111111111111111111111129
sleep 10
python tests/Zilliqa/test_zilliqa_local.py sendcmd 2 0108000000010100000001000000000000000000000000000000000000000000000000000000000000006400000000000000000000000000000000000000000000000000000000000000320D3979DA06841562C90DE5212BE5EFCF88FAEA17118945B6B49D304DE295E40700000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000005BA04A740D0FA29B841C6D99B02892273F7D00518EF12DAFA2AD4D198E630789CF3B00000002ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffed000000000000000000000000000000000000000000000000000000000000000A65F91F9DD0F49DCBCDA30D4FBB624A91C8074684372247E26D24FA9C63F9E67508090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F404142434445464792FF3F0FC76962D01403CC8C751949C002FECF6787F34CBB63224B5CE6D12012BB1B25C43EF0E19D3059115A3FA7DB1709298F2ED861FCE4E6BA2ED80DBB4CF7000000027F0B906245D429D46556C4BB4BC499A7EA2E05C650FB53754739856227CB3F220000000100000000000000000000000000000000000000000000000000000000000000050405060708090A0B0C0D0E0F101112131415161708090A0B0C0D0E0F101112131415161718191A1B0000000000000000000000000000000000000000000000000000000000000037101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F404142434445464748494A4B4C4D4E4F0308090A0B0C0D0E0F101112131415161718191A1B2100000000000000000000000000000000000000000000000000000000000000010405060708090A0B0C0D0E0F101112131415161708090A0B0C0D0E0F101112131415161718191A1B0000000000000000000000000000000000000000000000000000000000000021A9552A1921993CC7EF45A4BADCBBFC92EB7C7EF574E3FE485E81B6B9E52E42790000000100000000000000000000000000000000000000000000000000000000000000060102030405060708090A0B0C0D0E0F1011121314030405060708090A0B0C0D0E0F10111213141516000000000000000000000000000000000000000000000000000000000000000A05060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F404142434403030405060708090A0B0C0D0E0F101112131415162200000000000000000000000000000000000000000000000000000000000000010102030405060708090A0B0C0D0E0F1011121314030405060708090A0B0C0D0E0F10111213141516000000000000000000000000000000000000000000000000000000000000000A
python tests/Zilliqa/test_zilliqa_local.py sendcmd 7 0108000000000100000001000000000000000000000000000000000000000000000000000000000000006400000000000000000000000000000000000000000000000000000000000000320D3979DA06841562C90DE5212BE5EFCF88FAEA17118945B6B49D304DE295E40700000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000005BA14A740D0FA29B841C6D99B02892273F7D00518EF12DAFA2AD4D198E630789CF3B00000002ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeA000000000000000000000000000000000000000000000000000000000000000A65F91F9DD0F49DCBCDA30D4FBB624A91C8074684372247E26D24FA9C63F9E67508090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F404142434445464792FF3F0FC76962D01403CC8C751949C002FECF6787F34CBB63224B5CE6D12012BB1B25C43EF0E19D3059115A3FA7DB1709298F2ED861FCE4E6BA2ED80DBB4CF7000000027F0B906245D429D46556C4BB4BC499A7EA2E05C650FB53754739856227CB3F220000000100000000000000000000000000000000000000000000000000000000000000050405060708090A0B0C0D0E0F101112131415161708090A0B0C0D0E0F101112131415161718191A1B0000000000000000000000000000000000000000000000000000000000000037101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F404142434445464748494A4B4C4D4E4F0308090A0B0C0D0E0F101112131415161718191A1B2100000000000000000000000000000000000000000000000000000000000000010405060708090A0B0C0D0E0F101112131415161708090A0B0C0D0E0F101112131415161718191A1B0000000000000000000000000000000000000000000000000000000000000021A9552A1921993CC7EF45A4BADCBBFC92EB7C7EF574E3FE485E81B6B9E52E42790000000100000000000000000000000000000000000000000000000000000000000000060102030405060708090A0B0C0D0E0F1011121314030405060708090A0B0C0D0E0F10111213141516000000000000000000000000000000000000000000000000000000000000000A05060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F404142434403030405060708090A0B0C0D0E0F101112131415162200000000000000000000000000000000000000000000000000000000000000010102030405060708090A0B0C0D0E0F1011121314030405060708090A0B0C0D0E0F10111213141516000000000000000000000000000000000000000000000000000000000000000A
