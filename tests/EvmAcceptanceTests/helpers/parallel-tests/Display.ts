import clc from "cli-color";
import {performance} from "perf_hooks";

export class Chronometer {
  constructor() {
    this.startTime = 0;
    this.endTime = 0;
  }

  start() {
    this.startTime = performance.now();
  }

  finish() {
    this.endTime = performance.now();
  }

  display(): string {
    const time = (this.endTime - this.startTime) / 1000;
    const minutes = Math.floor(time / 60);
    const minutesString = `${minutes}`.padStart(2, "0");
    const seconds = `${(time - minutes * 60).toFixed(2)}`.padStart(2, "0");
    return `${minutesString}:${seconds}`;
  }

  startTime: number;
  endTime: number;
}

export const displayStageStarted = function (message: string) {
  console.log(clc.bold(message));
};

export const displayStageFinished = function (message: string, chronometer: Chronometer) {
  console.log(" ", clc.blackBright(message), "in", clc.yellow(chronometer.display()));
  console.log();
};

export const displayIgnored = function (message: string) {
  console.log(clc.yellowBright.bold("⚠️"), clc.blackBright(` ${message}`));
};
