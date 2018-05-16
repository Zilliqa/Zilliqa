/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/

#include "common/Constants.h"
#include "depends/common/CommonIO.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include <array>
#include <string>
#include <vector>

#define BOOST_TEST_MODULE contracttest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(contracttest)

// Init Account Store
BOOST_AUTO_TEST_CASE(initAccountStore)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    AccountStore::GetInstance().Init();
}

Address fromAddr;
Address toAddress;
KeyPair sender;
uint256_t nonce = 0;

string codeStr = R"((***************************************************)
(*               Associated library                *)
(***************************************************)
library Crowdfunding

let andb = 
  fun (b : Bool) => fun (c : Bool) =>
    match b with 
    | False => False
    | True  => match c with 
               | False => False
               | True  => True
               end
    end

let orb = 
  fun (b : Bool) => fun (c : Bool) =>
    match b with 
    | True  => True
    | False => match c with 
               | False => False
               | True  => True
               end
    end

let negb = fun (b : Bool) => 
  match b with
  | True => False
  | False => True
  end

let one_msg = 
  fun (msg : Message) => 
   let nil_msg = Nil {Message} in
   Cons {Message} msg nil_msg

let check_update = 
  fun (bs : Map Address Int) =>
    fun (sender : Address) =>
      fun (_amount : Int) =>
  let c = builtin contains bs sender in
  match c with 
  | False => 
    let bs1 = builtin put bs sender _amount in
    Some {Map Address Int} bs1 
  | True  => None {Map Address Int}
  end

let blk_leq =
  fun (blk1 : BNum) =>
  fun (blk2 : BNum) =>
  let bc1 = builtin blt blk1 blk2 in 
  let bc2 = builtin eq blk1 blk2 in 
  orb bc1 bc2

let accepted_code = 1
let missed_deadline_code = 2
let already_backed_code  = 3
let not_owner_code  = 4
let too_early_code  = 5
let got_funds_code  = 6
let cannot_get_funds  = 7
let cannot_reclaim_code = 8
let reclaimed_code = 9
  
(***************************************************)
(*             The contract definition             *)
(***************************************************)
contract Crowdfunding

(*  Parameters *)
(owner     : Address,
 max_block : BNum,
 goal      : Int)

(* Mutable fields *)
field backers : Map Address Int = Emp 
field funded : Bool = False

transition Donate (sender: Address, _amount: Int)
  blk <- & BLOCKNUMBER;
  in_time = blk_leq blk max_block;
  match in_time with 
  | True  => 
    bs  <- backers;
    res = check_update bs sender _amount;
    match res with
    | None => 
      msg  = {_tag : Main; to : sender; _amount : 0; 
              code : already_backed_code};
      msgs = one_msg msg;
      send msgs
    | Some bs1 =>
      backers := bs1; 
      accept; 
      msg  = {_tag : Main; to : sender; _amount : 0; 
              code : accepted_code};
      msgs = one_msg msg;
      send msgs     
     end  
  | False => 
    msg  = {_tag : Main; to : sender; _amount : 0; 
            code : missed_dealine_code};
    msgs = one_msg msg;
    send msgs
  end 
end

transition GetFunds (sender: Address)
  is_owner = builtin eq owner sender;
  match is_owner with
  | False => 
    msg  = {_tag : Main; to : sender; _amount : 0; 
            code : not_owner_code};
    msgs = one_msg msg;
    send msgs
  | True => 
    blk <- & BLOCKNUMBER;
    in_time = blk_leq blk max_block;
    c1 = negb in_time;
    bal <- balance;
    c2 = builtin lt bal goal;
    c3 = negb c2;
    c4 = andb c1 c3;
    match c4 with 
    | False =>  
      msg  = {_tag : Main; to : sender; _amount : 0; 
              code : cannot_get_funds};
      msgs = one_msg msg;
      send msgs
    | True => 
      tt = True;
      funded := tt;
      msg  = {_tag : Main; to : owner; _amount : bal; 
              code : got_funds_code};
      msgs = one_msg msg;
      send msgs
    end
  end   
end

(* transition ClaimBack *)
transition ClaimBack (sender: Address)
  blk <- & BLOCKNUMBER;
  after_deadline = builtin blt max_block blk;
  match after_deadline with
  | False =>
      msg  = {_tag : Main; to : sender; _amount : 0; 
              code : too_early_code};
      msgs = one_msg msg;
      send msgs
  | True =>
    bs <- backers;
    bal <- balance;
    (* Goal has not been reached *)
    f <- funded;
    c1 = builtin lt bal goal;
    c2 = builtin contains bs sender;
    c3 = negb f;
    c4 = andb c1 c2;
    c5 = andb c3 c4;
    match c5 with
    | False =>
        msg  = {_tag : Main; to : sender; _amount : 0; 
                code : cannot_reclaim_code};
        msgs = one_msg msg;
        send msgs
    | True =>
      res = builtin get bs sender;
      match res with
      | None =>
          msg  = {_tag : Main; to : sender; _amount : 0; 
                  code : cannot_reclaim_code};
          msgs = one_msg msg;
          send msgs
      | Some v =>
          bs1 = builtin remove bs sender;
	  backers := bs1;
          msg  = {_tag : Main; to : sender; _amount : v; 
                  code : reclaimed_code};
          msgs = one_msg msg;
          send msgs
      end
    end
  end  
end)";

string initStr = R"([
    {
        "vname" : "owner",
        "type" : "Address", 
        "value" : "0x1234567890123456789012345678901234567890"
    },
    {
        "vname" : "max_block",
        "type" : "BNum" ,
        "value" : "199"
    },
    { 
        "vname" : "goal",
        "type" : "Int",
        "value" : "500"
    }
])";

string dataStr = R"({
    "_tag": "Donate",
    "_amount": "100",
    "params": [
      {
        "vname": "sender",
        "type": "Address",
        "value": "0x12345678901234567890123456789012345678ab"
      }
    ]
}
)";

string dataStr2 = R"({
    "_tag": "Donate",
    "_amount": "200",
    "params": [
      {
        "vname": "sender",
        "type": "Address",
        "value": "0x12345678901234567890123456789012345678cd"
      }
    ]
})";

string dataStr4 = R"({
    "_tag": "GetFunds",
    "_amount": "0",
    "params": [
      {
        "vname": "sender",
        "type": "Address",
        "value": "0x12345678901234567890123456789012345678cd"
      }
    ]
})";

string outStr = R"({
  "message": {
    "_tag": "Main",
    "_amount": "0",
    "params": [
      {
        "vname": "to",
        "type": "Address",
        "value": "0x12345678901234567890123456789012345678ab"
      },
      { "vname": "code", "type": "Int", "value": "1" }
    ]
  },
  "states": [
    { "vname": "_balance", "type": "Int", "value": "400" },
    {
      "vname": "backers",
      "type": "Map",
      "value": [
        { "keyType": "Address", "valType": "Int" },
        { "key": "0x12345678901234567890123456789012345678ab", "val": "100" }
      ]
    },
    {
      "vname": "funded",
      "type": "ADT",
      "value": { "constructor": "False", "argtypes": [], "arguments": [] }
    }
  ]
})";

// Create Transaction to create contract
BOOST_AUTO_TEST_CASE(createContract)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    sender = Schnorr::GetInstance().GenKeyPair();

    std::vector<unsigned char> vec;
    sender.second.Serialize(vec, 0);
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(vec);
    const std::vector<unsigned char>& output = sha2.Finalize();

    copy(output.end() - ACC_ADDR_SIZE, output.end(),
         fromAddr.asArray().begin());

    std::vector<unsigned char> code(codeStr.begin(), codeStr.end());
    std::vector<unsigned char> data(initStr.begin(), initStr.end());

    toAddress = NullAddress;

    Transaction tx1(1, nonce, toAddress, sender, 200, 11, 66, code, data);

    AccountStore::GetInstance().UpdateAccounts(1, tx1);

    toAddress = Account::GetAddressForContract(fromAddr, nonce);

    bool checkToAddr = true;
    Account* account = AccountStore::GetInstance().GetAccount(toAddress);
    if (account == nullptr)
    {
        checkToAddr = false;
    }
    BOOST_CHECK_MESSAGE(checkToAddr, "Error with creation of contract account");
}

// Create Transaction to call contract
BOOST_AUTO_TEST_CASE(callContract)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    std::vector<unsigned char> data(dataStr.begin(), dataStr.end());

    std::vector<unsigned char> vec;
    Transaction tx2(1, nonce, toAddress, sender, 100, 11, 66, vec, data);
    AccountStore::GetInstance().UpdateAccounts(1, tx2);

    outStr.erase(std::remove(outStr.begin(), outStr.end(), ' '), outStr.end());
    outStr.erase(std::remove(outStr.begin(), outStr.end(), '\n'), outStr.end());

    ifstream infile{OUTPUT_JSON};
    std::string output_file{istreambuf_iterator<char>(infile),
                            istreambuf_iterator<char>()};

    output_file.erase(std::remove(output_file.begin(), output_file.end(), ' '),
                      output_file.end());
    output_file.erase(std::remove(output_file.begin(), output_file.end(), '\n'),
                      output_file.end());

    BOOST_CHECK_MESSAGE(outStr == output_file,
                        "Error: didn't get desired output");
}

// BOOST_AUTO_TEST_CASE(callContract2)
// {
//     INIT_STDOUT_LOGGER();

//     LOG_MARKER();

//     std::vector<unsigned char> data(dataStr2.begin(), dataStr2.end());

//     std::vector<unsigned char> vec;
//     Transaction tx3(1, nonce, toAddress, sender, 100, 11, 66, vec, data);
//     AccountStore::GetInstance().UpdateAccounts(1, tx3);
// }

// BOOST_AUTO_TEST_CASE(callContract4)
// {
//     INIT_STDOUT_LOGGER();

//     LOG_MARKER();

//     std::vector<unsigned char> data(dataStr4.begin(), dataStr4.end());

//     std::vector<unsigned char> vec;
//     Transaction tx4(1, nonce, toAddress, sender, 100, 11, 66, vec, data);
//     AccountStore::GetInstance().UpdateAccounts(1, tx4);
// }

BOOST_AUTO_TEST_SUITE_END()
