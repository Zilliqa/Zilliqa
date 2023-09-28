import clc from "cli-color";
import ora from "ora";
import mocha, {Test} from "mocha";
const {EVENT_RUN_END, EVENT_TEST_FAIL, EVENT_TEST_PASS, EVENT_TEST_PENDING} = mocha.Runner.constants;

module.exports = MyReporter;

function MyReporter(runner) {
  mocha.reporters.Base.call(this, runner);
  let passes = 0;
  let disabled = 0;
  let failures = 0;
  let timeouts = 0;
  const spinner = ora();

  function totalTests(): number {
    return passes + disabled + failures + timeouts;
  }

  function resultsString(delim: string): string {
    return (
      clc.bold.white(`Total: ${totalTests()}`) +
      delim +
      clc.bold.green(`Passed: ${passes}`) +
      delim +
      clc.bold.red(`Failed: ${failures}`) +
      delim +
      clc.bold.yellow(`Timed out: ${timeouts}`) +
      delim +
      clc.bold.cyan(`Disabled: ${disabled}`)
    );
  }

  runner.on(EVENT_TEST_PASS, function (test: Test) {
    passes++;
    spinner.start(resultsString("   "));
  });

  runner.on(EVENT_TEST_PENDING, function (test: Test) {
    disabled++;
    spinner.start(resultsString("   "));
  });

  runner.on(EVENT_TEST_FAIL, function (test: Test, err) {
    spinner.clear();
    console.log();
    if (err.code === "ERR_MOCHA_TIMEOUT") {
      timeouts++;
      console.log(clc.yellow(`${timeouts}) ${test.parent?.title}`));
      console.log("  ", clc.yellow(test.title));
    } else {
      failures++;
      console.log(clc.red(`${failures}) ${test.parent?.title}`));
      console.log("  ", clc.red(test.title));
      if (err.actual !== undefined && err.expected !== undefined) {
        console.log("  ", mocha.reporters.Base.generateDiff(err.actual, err.expected));
      }
      console.log("  ", clc.blackBright(err.stack));
    }
    spinner.start(resultsString("   "));
  });

  runner.on(EVENT_RUN_END, function () {
    spinner.clear();
    console.log();
    console.log(resultsString("\n"));
    console.log();
  });
}
