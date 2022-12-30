import hre from "hardhat";

async function main() {
  while (true) {
    await hre.run("test", {grep: "@transactional"});

    // Sleep for 10 seconds
    await new Promise((resolve) => setTimeout(resolve, 10_000));
  }
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
