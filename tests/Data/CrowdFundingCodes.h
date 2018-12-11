/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */
#include <string>

std::string cfCodeStr = R"(scilla_version 0

(***************************************************)
(*               Associated library                *)
(***************************************************)

import BoolUtils

library Crowdfunding

let one_msg = 
  fun (msg : Message) => 
    let nil_msg = Nil {Message} in
    Cons {Message} msg nil_msg


let check_update = 
  fun (bs : Map ByStr20 Uint128) =>
  fun (_sender : ByStr20) =>
  fun (_amount : Uint128) =>
    let c = builtin contains bs _sender in
    match c with 
    | False => 
      let bs1 = builtin put bs _sender _amount in
      Some {Map ByStr20 Uint128} bs1 
    | True  => None {Map ByStr20 Uint128}
    end

let blk_leq =
  fun (blk1 : BNum) =>
  fun (blk2 : BNum) =>
    let bc1 = builtin blt blk1 blk2 in 
    let bc2 = builtin eq blk1 blk2 in 
    orb bc1 bc2

let accepted_code = Int32 1
let missed_deadline_code = Int32 2
let already_backed_code  = Int32 3
let not_owner_code  = Int32 4
let too_early_code  = Int32 5
let got_funds_code  = Int32 6
let cannot_get_funds  = Int32 7
let cannot_reclaim_code = Int32 8
let reclaimed_code = Int32 9
  
(***************************************************)
(*             The contract definition             *)
(***************************************************)
contract Crowdfunding

(*  Parameters *)
(owner     : ByStr20,
 max_block : BNum,
 goal      : Uint128)

(* Mutable fields *)
field backers : Map ByStr20 Uint128 = Emp ByStr20 Uint128
field funded : Bool = False

transition Donate ()
  blk <- & BLOCKNUMBER;
  in_time = blk_leq blk max_block;
  match in_time with 
  | True  => 
    bs  <- backers;
    res = check_update bs _sender _amount;
    match res with
    | None => 
      e = {_eventname : "DonationFailure"; donor : _sender; amount : _amount; code : already_backed_code};
      event e
    | Some bs1 =>
      backers := bs1; 
      accept; 
      e = {_eventname : "DonationSuccess"; donor : _sender; amount : _amount; code : accepted_code};
      event e
    end  
  | False => 
  e = {_eventname : "DonationFailure"; donor : _sender; amount : _amount; code : missed_deadline_code};
    event e
  end 
end

transition GetFunds ()
  is_owner = builtin eq owner _sender;
  match is_owner with
  | False =>
  e = {_eventname : "GetFundsFailure"; caller : _sender; amount : Uint128 0; code : not_owner_code};
    event e
  | True => 
    blk <- & BLOCKNUMBER;
    in_time = blk_leq blk max_block;
    c1 = negb in_time;
    bal <- _balance;
    c2 = builtin lt bal goal;
    c3 = negb c2;
    c4 = andb c1 c3;
    match c4 with 
    | False =>  
    e = {_eventname : "GetFundsFailure"; caller : _sender; amount : Uint128 0; code : cannot_get_funds};
      event e
    | True => 
      tt = True;
      funded := tt;
      msg  = {_tag : ""; _recipient : owner; _amount : bal}; 
    msgs = one_msg msg;
    e = {_eventname : "GetFundsSuccess"; caller : owner; amount : bal; code : got_funds_code};
      event e;
    send msgs
    end
  end   
end

(* transition ClaimBack *)
transition ClaimBack ()
  blk <- & BLOCKNUMBER;
  after_deadline = builtin blt max_block blk;
  match after_deadline with
  | False =>
  e = { _eventname : "ClaimBackFailure"; caller : _sender; amount : Uint128 0; code : too_early_code};
    event e
  | True =>
    bs <- backers;
    bal <- _balance;
    (* Goal has not been reached *)
    f <- funded;
    c1 = builtin lt bal goal;
    c2 = builtin contains bs _sender;
    c3 = negb f;
    c4 = andb c1 c2;
    c5 = andb c3 c4;
    match c5 with
    | False =>
    e = { _eventname : "ClaimBackFailure"; caller : _sender; amount : Uint128 0; code : cannot_reclaim_code};
      event e
    | True =>
      res = builtin get bs _sender;
      match res with
      | None =>
      e = { _eventname : "ClaimBackFailure"; caller : _sender; amount : Uint128 0; code : cannot_reclaim_code};
        event e
      | Some v =>
        bs1 = builtin remove bs _sender;
        backers := bs1;
      msg  = {_tag : ""; _recipient : _sender; _amount : v};
      msgs = one_msg msg;
      e = { _eventname : "ClaimBackSuccess"; caller : _sender; amount : v; code : reclaimed_code};
        event e;
      send msgs
      end
    end
  end  
end)";

std::string cfInitStr = R"([
    {
        "vname" : "owner",
        "type" : "ByStr20", 
        "value" : "$ADDR"
    },
    {
        "vname" : "max_block",
        "type" : "BNum" ,
        "value" : "199"
    },
    { 
        "vname" : "goal",
        "type" : "Uint128",
        "value" : "500"
    }
])";

std::string cfDataDonateStr = R"({
    "_tag": "Donate",
    "params": []
}
)";

std::string cfDataClaimBackStr = R"({
    "_tag": "ClaimBack",
    "params": []
})";

std::string cfDataGetFundsStr = R"({
    "_tag": "GetFunds",
    "params": []
})";

std::string cfOutStr0 = R"({
  "scilla_major_version": "0",
  "gas_remaining": "293",
  "_accepted": "false",
  "message": null,
  "states": [],
  "events": []
})";

std::string cfOutStr1 = R"({
  "scilla_major_version": "0",
  "gas_remaining": "4373",
  "_accepted": "true",
  "message": null,
  "states": [
    { "vname": "_balance", "type": "Uint128", "value": "100" },
    {
      "vname": "backers",
      "type": "Map (ByStr20) (Uint128) ",
      "value": [
        { "key": "0x5c6712c8f3b049e98e733cfdb38a8e37a1c724c0", "val": "100" }
      ]
    },
    {
      "vname": "funded",
      "type": "Bool",
      "value": { "constructor": "False", "argtypes": [], "arguments": [] }
    }
  ],
  "events": [
    {
      "_eventname": "DonationSuccess",
      "params": [
        {
          "vname": "donor",
          "type": "ByStr20",
          "value": "0x5c6712c8f3b049e98e733cfdb38a8e37a1c724c0"
        },
        { "vname": "amount", "type": "Uint128", "value": "100" },
        { "vname": "code", "type": "Int32", "value": "1" }
      ]
    }
  ]
}
)";

std::string cfOutStr2 = R"({
  "scilla_major_version": "0",
  "gas_remaining": "4264",
  "_accepted": "true",
  "message": null,
  "states": [
    { "vname": "_balance", "type": "Uint128", "value": "300" },
    {
      "vname": "backers",
      "type": "Map (ByStr20) (Uint128) ",
      "value": [
        { "key": "0x0287e3c3e69cd86102e29cc80563a4811b79ee55", "val": "200" },
        { "key": "0x5c6712c8f3b049e98e733cfdb38a8e37a1c724c0", "val": "100" }
      ]
    },
    {
      "vname": "funded",
      "type": "Bool",
      "value": { "constructor": "False", "argtypes": [], "arguments": [] }
    }
  ],
  "events": [
    {
      "_eventname": "DonationSuccess",
      "params": [
        {
          "vname": "donor",
          "type": "ByStr20",
          "value": "0x0287e3c3e69cd86102e29cc80563a4811b79ee55"
        },
        { "vname": "amount", "type": "Uint128", "value": "200" },
        { "vname": "code", "type": "Int32", "value": "1" }
      ]
    }
  ]
})";

std::string cfOutStr3 = R"({
  "scilla_major_version": "0",
  "gas_remaining": "4441",
  "_accepted": "false",
  "message": null,
  "states": [
    { "vname": "_balance", "type": "Uint128", "value": "300" },
    {
      "vname": "backers",
      "type": "Map (ByStr20) (Uint128) ",
      "value": [
        { "key": "0x0287e3c3e69cd86102e29cc80563a4811b79ee55", "val": "200" },
        { "key": "0x5c6712c8f3b049e98e733cfdb38a8e37a1c724c0", "val": "100" }
      ]
    },
    {
      "vname": "funded",
      "type": "Bool",
      "value": { "constructor": "False", "argtypes": [], "arguments": [] }
    }
  ],
  "events": [
    {
      "_eventname": "GetFundsFailure",
      "params": [
        {
          "vname": "caller",
          "type": "ByStr20",
          "value": "0x0287e3c3e69cd86102e29cc80563a4811b79ee55"
        },
        { "vname": "amount", "type": "Uint128", "value": "0" },
        { "vname": "code", "type": "Int32", "value": "4" }
      ]
    }
  ]
})";

std::string cfOutStr4 = R"({
  "scilla_major_version": "0",
  "gas_remaining": "4137",
  "_accepted": "false",
  "message": {
    "_tag": "",
    "_amount": "100",
    "_recipient": "0x5c6712c8f3b049e98e733cfdb38a8e37a1c724c0",
    "params": []
  },
  "states": [
    { "vname": "_balance", "type": "Uint128", "value": "200" },
    {
      "vname": "backers",
      "type": "Map (ByStr20) (Uint128) ",
      "value": [
        { "key": "0x0287e3c3e69cd86102e29cc80563a4811b79ee55", "val": "200" }
      ]
    },
    {
      "vname": "funded",
      "type": "Bool",
      "value": { "constructor": "False", "argtypes": [], "arguments": [] }
    }
  ],
  "events": [
    {
      "_eventname": "ClaimBackSuccess",
      "params": [
        {
          "vname": "caller",
          "type": "ByStr20",
          "value": "0x5c6712c8f3b049e98e733cfdb38a8e37a1c724c0"
        },
        { "vname": "amount", "type": "Uint128", "value": "100" },
        { "vname": "code", "type": "Int32", "value": "9" }
      ]
    }
  ]
}
)";