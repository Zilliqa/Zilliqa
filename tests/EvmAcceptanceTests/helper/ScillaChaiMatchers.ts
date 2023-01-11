var chaiSubset = require("chai-subset");
import chai from "chai";
chai.use(chaiSubset);

import {Transaction} from "@zilliqa-js/account";

export interface EventParam {
  type?: string;
  value?: string;
  vname?: string;
}

declare global {
  export namespace Chai {
    interface Assertion {
      sendEvent(eventName: string): Promise<void>;
      sendEventWithParams(eventName: string, ...params: EventParam[]): Promise<void>;
    }
  }
}

export const scillaChaiEventMatcher = function (chai: Chai.ChaiStatic, utils: Chai.ChaiUtils) {
  var Assertion = chai.Assertion;
  Assertion.addMethod("sendEvent", function (eventName: string) {
    var tx: Transaction = this._obj;

    const receipt = tx.getReceipt()!;
    new Assertion(receipt.event_logs).not.to.be.null;

    const event_logs = receipt.event_logs!;

    new Assertion(event_logs.map(({_eventname}) => _eventname)).to.contain(eventName);
  });

  Assertion.addMethod("sendEventWithParams", function (eventName: string, ...params: EventParam[]) {
    var tx: Transaction = this._obj;

    const receipt = tx.getReceipt()!;
    new Assertion(this._obj).to.sendEvent(eventName);

    const event_logs = receipt.event_logs!;
    const desiredLog = event_logs.filter((log) => log._eventname === eventName);

    new Assertion(desiredLog[0].params).to.containSubset(params);
  });
};
