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
#include <string>

std::string icfCodeStr = R"(library CrowdFundingInvoke

let one_msg = 
  fun (msg : Message) => 
    let nil_msg = Nil {Message} in
      Cons {Message} msg nil_msg

  
(***************************************************)
(*             The contract definition             *)
(***************************************************)
contract CrowdFundingInvoke

(*  Parameters *)
(cfaddr     : Address) (* address of the crowdfunding contract *)

(* Mutable fields *)
(* callers only keeps track of who all called Invoke. No real use *)
field callers : Map Address Int = Emp Address Int

transition Invoke (trans : String)
  bal <- balance;
  s = _sender;
  donate_s = "Donate";
  is_donate = builtin eq trans donate_s;
  match is_donate with
  | True =>
    msg = {_tag : Donate; _recipient : cfaddr; _amount : bal};
    msgs = one_msg msg;
    send msgs
  | False =>
    claimback_s = "ClaimBack";
    is_claimback = builtin eq trans claimback_s;
    match is_claimback with
    | True =>
      msg = {_tag : ClaimBack; _recipient : cfaddr; _amount : 0};
      msgs = one_msg msg;
      send msgs
    | False =>
      getfunds_s = "GetFunds";
      is_getfunds = builtin eq trans getfunds_s;
      match is_getfunds with
      | True =>
        msg = {_tag : GetFunds; _recipient : cfaddr; _amount : 0};
        msgs = one_msg msg;
        send msgs
      | False =>
        msg = {_tag : Main; _recipient : _sender ; _amount : 0};
        msgs = one_msg msg;
        send msgs
      end
    end
  end
end)";

std::string icfInitStr = R"([
    {
        "vname" : "cfaddr",
        "type" : "Address", 
        "value" : "$ADDR"
    }
])";

std::string icfDataStr1 = R"({
    "_tag": "Invoke",
    "params": [
      {
        "vname": "trans",
        "type": "String",
        "value": "Donate"
      }
    ]
})";

std::string icfDataStr2 = R"({
    "_tag": "Invoke",
    "params": [
      {
        "vname": "trans",
        "type": "String",
        "value": "ClaimBack"
      }
    ]
})";

std::string icfDataStr3 = R"({
    "_tag": "Invoke",
    "params": [
      {
        "vname": "trans",
        "type": "String",
        "value": "GetFunds"
      }
    ]
})";

std::string icfOutStr1 = R"({
  "message": {
    "_tag": "Donate",
    "_amount": "122",
    "params": [
      {
        "vname": "to",
        "type": "Address",
        "value": "$ADDR"
      }
    ]
  },
  "states": [
    { "vname": "_balance", "type": "Int", "value": "0" },
    { "vname": "callers", "type": "Map", "value": null }
  ]
})";

std::string icfOutStr2 = R"({
  "message": {
    "_tag": "ClaimBack",
    "_amount": "0",
    "params": [
      {
        "vname": "to",
        "type": "Address",
        "value": "$ADDR"
      }
    ]
  },
  "states": [
    { "vname": "_balance", "type": "Int", "value": "122" },
    { "vname": "callers", "type": "Map", "value": null }
  ]
})";

std::string icfOutStr3 = R"({
  "message": {
    "_tag": "GetFunds",
    "_amount": "0",
    "params": [
      {
        "vname": "to",
        "type": "Address",
        "value": "$ADDR"
      }
    ]
  },
  "states": [
    { "vname": "_balance", "type": "Int", "value": "122" },
    { "vname": "callers", "type": "Map", "value": null }
  ]
})";