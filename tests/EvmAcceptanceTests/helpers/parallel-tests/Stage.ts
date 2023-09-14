import ora from "ora";
import {Chronometer, displayStageFinished} from "./Display";
import clc from "cli-color";

export const runStage = async (name: string, stage: Function, done: Function, ...params: any[]) => {
  const spinner = ora();
  spinner.start(clc.bold(name));
  const chronometer = new Chronometer();
  chronometer.start();
  const output = await stage(...params);
  chronometer.finish();
  const {finished_message, success} = done(params, output);
  if (success) {
    spinner.succeed();
  } else {
    spinner.fail();
  }
  displayStageFinished(finished_message, chronometer);
  return output;
};
