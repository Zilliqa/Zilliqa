import {BigNumber} from "ethers";

export type Event = {
  from: string;
  to: string;
  amount: BigNumber;
};

// Ensures all events in current eventloop run are dispatched
export async function waitForEvents(events: Event[], timeout = 5000) {
  await new Promise((r) => setTimeout(r, timeout));
  return events;
}
