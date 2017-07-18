import argparse
import os
import urllib.request
import re
import shutil
import sys

sources_txt = os.path.join('etc', 'xpt', 'sources.txt')
session_dir = os.path.join('var', 'xpt', 'session')
session_tags_dir = os.path.join(session_dir, 'tags')


class Session:
    def __init__(self):
        pass

    def update(self):
        if os.path.exists(session_dir):
            shutil.rmtree(session_dir)
        os.makedirs(session_tags_dir)
        for line in open(sources_txt).read().splitlines():
            if re.match('^repo ', line):
                chunks = line.split()
                url = chunks[1]
                tags = chunks[2:] if len(chunks) > 2 else ['']
                for tag in tags:
                    packages_txt_url = url + '/' + tag + '/packages.txt'
                    print('get ' + packages_txt_url)
                    with open(os.path.join(session_tags_dir, tag if tag else '_no_tag_'), 'a') as f:
                        for package in urllib.request.urlopen(packages_txt_url).read().decode().splitlines():
                            f.write(url + '/' + package + '\n')
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

