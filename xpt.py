import argparse
import sys


class Session:
    def __init__(self):
        pass

    def update(self):
        return 0

    def install(self, nargs):
        return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='xpt')
    parser.add_argument('--update', action='store_true')
    parser.add_argument('--install', nargs='+')
    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(1)
    args = parser.parse_args()

    session = Session()

    if args.update:
        sys.exit(session.update())

    if args.install:
        sys.exit(session.install(args.install))

