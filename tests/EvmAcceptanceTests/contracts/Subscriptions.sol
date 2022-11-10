// SPDX-License-Identifier: GPL-3.0-or-later

pragma solidity >=0.7.0 <0.9.0;
 
contract Subscriptions {
    event Event0(address from, address to, uint tokens);
    event Event1(address indexed from, address to, uint tokens);
    event Event2(address indexed from, address indexed to, uint tokens);
    event Event3(address indexed from, address indexed to, uint indexed tokens);
 
    function event0(address to, uint tokens) public virtual {
        emit Event0(msg.sender, to, tokens);
    }

    function event1(address to, uint tokens) public virtual {
        emit Event1(msg.sender, to, tokens);
    }

    function event2(address to, uint tokens) public virtual {
        emit Event2(msg.sender, to, tokens);
    }

    function event3(address to, uint tokens) public virtual {
        emit Event3(msg.sender, to, tokens);
    } 
}