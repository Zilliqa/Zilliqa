class Data:
    def __init__(self) -> object:
        self.normal_ips_from_origin = []
        self.lookup_ips_from_origin = []
        self.multiplier_ips_from_origin = []
        self.seedpub_ips_from_origin = []
        self.guard_ips_from_origin = []
        # ports work in ranges, each service gets 5 ports, so step range by 5
        # reserve 4 extra for other services such as status api etc.
        self.normal_port = 50100
        self.guard_port = 50150
        self.lookup_port = 50200
        self.seedpub_port = 50300
        self.multiplier_port = 50400
        self.my_ip = "192.168.0.98"
        self.origin_server = ""

    def __str__(self):
        return f"normal: {self.normal_ips_from_origin}\nlookup: {self.lookup_ips_from_origin}\nmultiplier: {self.multiplier_ips_from_origin}\nSeedpub:{self.seedpub_ips_from_origin}\nGaurd:{self.gaurd_ips_from_origin}\n"

    def get_ips_list_from_pseudo_origin(self, arg) -> bool:
        try:
            # lookup
            self.lookup_ips_from_origin = list(
                zip([str(self.my_ip)] * arg.l, range(self.lookup_port, self.lookup_port + arg.l, 5),
                    arg.lookup_keypairs))
            # seedpub
            self.seedpub_ips_from_origin = list(zip([str(self.my_ip)] * len(arg.seedpub_keypairs),
                                                    range(self.seedpub_port,
                                                          self.seedpub_port + len(arg.seedpub_keypairs), 5),
                                                    arg.seedpub_keypairs))
            # multiplier
            self.multiplier_ips_from_origin = list(zip([str(self.my_ip)] * len(arg.multiplier_keypairs),
                                                       range(self.multiplier_port,
                                                             self.multiplier_port + len(arg.multiplier_keypairs), 5),
                                                       arg.multiplier_keypairs))

            # now normals

            self.guard_ips_from_origin = list(zip([str(self.my_ip)] * arg.ds_guard,
                                                  range(self.guard_port,
                                                        self.guard_port + (5 * arg.ds_guard), 5),
                                                  arg.keypairs[0:arg.ds_guard]))

            keys = arg.keypairs[arg.d:arg.n]
            ports = range(self.normal_port, self.normal_port + arg.n - arg.d, 5)
            ips = [str(self.my_ip)] * (arg.n - arg.ds_guard)
            self.normal_ips_from_origin = list(zip(ips, ports, keys))

        except Exception as _:
            print("We suspect the arguments are suspect")
            return False

        return True
