from clavier import arg_par


def add_parser(subparsers: arg_par.Subparsers):
    subparsers.add_children(__name__, __path__)
    # for module in dyn.children_modules(__name__, __path__):
    #     if hasattr(module, "add_to"):
    #         module.add_to(subparsers)
