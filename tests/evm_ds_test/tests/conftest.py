import pytest
import os
import numpy
import random as rand

@pytest.fixture(scope="session", autouse=True)
def random():
    seed = int(os.environ.get("TESTSEED", "0"))
    print("TEST RANDOM SEED IS {}".format(seed))
    rand.seed(seed)
    numpy.random.seed(seed)
