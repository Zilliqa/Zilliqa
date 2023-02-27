// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

// Entry contract, will call contract two
contract ContractOne {

    event FinalMessage();

    constructor() payable
    {
    }

    function chainedCall(address payable[] memory destinations, uint index) public returns(bool success)
    {

        if (destinations.length > index) {
            ContractTwo two = ContractTwo(destinations[index]);

            two.chainedCall(destinations, index + 1);
        }
        emit FinalMessage();
        return true;
    }
}

// Second contract, will call contract three, twice
contract ContractTwo {

    event FinalMessageTwo();

    constructor() payable
    {
    }

    function chainedCall(address payable[] memory destinations, uint index) public returns(bool success)
    {

        // Contract two will call three TWICE
        if (destinations.length > index) {
            ContractThree three = ContractThree(destinations[index]);
            three.chainedCall(destinations, index + 1);
            three.chainedCall(destinations, index + 1);
        }
        emit FinalMessageTwo();
        return true;
    }
}

// Last contract, will call contract one
contract ContractThree {

    event FinalMessageThree();

    constructor() payable
    {
    }

    function chainedCall(address payable[] memory destinations, uint index) public returns(bool success)
    {

        // Contract two will call three twice
        if (destinations.length > index) {
            ContractOne one = ContractOne(destinations[index]);
            one.chainedCall(destinations, index + 1);
        }
        emit FinalMessageThree();
        return true;
    }
}
