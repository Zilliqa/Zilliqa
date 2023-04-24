// SPDX-License-Identifier: SEE LICENSE IN LICENSE
pragma solidity ^0.8.0;

contract TestDelegatecall {
    address public sender;
    uint public value;
    uint public num;

    function setVars(uint _num) external payable {

        require(num == 3735928559, "Didn't reflect variables from calling contract correctly"); // 0xDEADBEEF

        sender = msg.sender;
        value = msg.value;
        num = _num;
    }

    function zero() external payable {
      value = 0;
      num = 0;
      sender = address(0);
    }

    function setNoVars(address _sender, uint _num) external payable {
      // require(num == 0, "Did reflect variables from calling contract correctly");
      sender = msg.sender;
      value = msg.value;
      num = 42;

      (bool success, bytes memory data) = _sender.delegatecall(
          abi.encodeWithSelector(Delegatecall.setVarsCallback.selector, _num));
      require(num == _num, "Didn't set it");
      require(success, "delegatecall failed");
    }
}

contract Delegatecall {
    address public sender;
    uint public value;
    uint public num;

    function setVars(address _test, uint _num) external payable {

        num = 3735928559; // 0xDEADBEEF
        value = 4027445261; // 0xF00DF00D

        (bool success, bytes memory data) = _test.delegatecall(
            abi.encodeWithSelector(TestDelegatecall.setVars.selector, _num)
        );

        require(num == _num, "Didn't set it...");
        require(success, "delegatecall failed");
    }

    function setVarsCallback(uint _num) external payable {
      //require(num == 42, "Didn't reflect indirect variables correctly");
      sender = msg.sender;
      value = msg.value;
      num = _num;
    }

    function setVarsCall(address _test, uint _num) external payable {
        num = 3735928559; // 0xDEADBEEF
        value = 4027445261; // 0xF00DF00D

        _test.call(
            abi.encodeWithSelector(TestDelegatecall.setNoVars.selector, _test, _num)
        );

        require(num == 3735928559, "Corrupted local state...");
    }

    function unwind(address payable _me, address _test) external {
      _me.transfer(address(this).balance);
      TestDelegatecall(_test).zero();
    }
}
