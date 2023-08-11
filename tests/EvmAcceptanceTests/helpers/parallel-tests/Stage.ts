import ora from "ora";
import { Chronometer, displayStageFinished } from "./Display";
import clc from "cli-color";


export const runStage = async(name: string, stage: Function, done: Function, ...params: any[]) => {
  const spinner = ora();
  spinner.start(clc.bold(name));
  const chronometer = new Chronometer();
  chronometer.start();
  const output = await stage(...params);
  chronometer.finish();
  spinner.succeed();
  const finished_message = done(params, output);
  displayStageFinished(finished_message, chronometer);
  return output;
}