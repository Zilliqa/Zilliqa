import yargs from "yargs/yargs";

export const ENV_VARS = yargs()
  .env()
  .options({
    debug: {
      type: "boolean",
      default: false
    },
    mochaWorkers: {
      type: "number",
      default: 4
    },
    mochaTimeout: {
      type: "number",
      default: 300000
    },
    scilla: {
      type: "boolean",
      default: true
    },
    ethernalEmail: {
      type: "string"
    },
    ethernalPassword: {
      type: "string"
    },
    ethernalWorkspace: {
      type: "string"
    }
  })
  .parseSync();
