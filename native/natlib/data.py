class Data:
    def __init__(self) -> object:
        self.normal_ips_from_origin = []
        self.lookup_ips_from_origin = []
        self.multiplier_ips_from_origin = []
        self.seedpub_ips_from_origin = []
        self.guard_ips_from_origin = []
        self.normal_port = 4100
        self.guard_port = 4150
        self.lookup_port = 4200
        self.seedpub_port = 4300
        self.multiplier_port = 4400
        self.my_ip = "0.0.0.0"
        self.origin_server = ""

    def __str__(self):
        return f"normal: {self.normal_ips_from_origin}\nlookup: {self.lookup_ips_from_origin}\nmultiplier: {self.multiplier_ips_from_origin}\nSeedpub:{self.seedpub_ips_from_origin}\nGaurd:{self.gaurd_ips_from_origin}\n"

    def get_ips_list_from_pseudo_origin(self, arg) -> bool:
        try:
            # lookup
            self.lookup_ips_from_origin = list(
                zip([str(self.my_ip)] * arg.l, range(self.lookup_port, self.lookup_port + arg.l), arg.lookup_keypairs))
            # seedpub
            self.seedpub_ips_from_origin = list(zip([str(self.my_ip)] * len(arg.seedpub_keypairs),
                                                    range(self.seedpub_port,
                                                          self.seedpub_port + len(arg.seedpub_keypairs)),
                                                    arg.seedpub_keypairs))
            # multiplier
            self.multiplier_ips_from_origin = list(zip([str(self.my_ip)] * len(arg.multiplier_keypairs),
                                                       range(self.multiplier_port,
                                                             self.multiplier_port + len(arg.multiplier_keypairs)),
                                                       arg.multiplier_keypairs))
            # normal processes take first few slots
            keys = arg.keypairs[:arg.n - arg.ds_guard]
            ports = range(self.normal_port, self.normal_port + arg.n - arg.ds_guard)
            ips = [str(self.my_ip)] * (arg.n - arg.ds_guard)

            self.normal_ips_from_origin = list(zip(ips, ports, keys))

            # gaurds take the remainder of the slots
            self.guard_ips_from_origin = list(zip([str(self.my_ip)] * arg.ds_guard,
                                                  range(self.normal_port + arg.n - arg.ds_guard,
                                                        self.normal_port + arg.n - arg.ds_guard + arg.ds_guard),
                                                  arg.keypairs[arg.n - arg.ds_guard:]))

        except Exception as _:
            print("We suspect the arguments are suspect")
            return False

        return True
