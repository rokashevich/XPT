import argparse
import os
import urllib.request
import re
import shutil
import sys

sources_txt = os.path.join('etc', 'xpt', 'sources.txt')
session_dir = os.path.join('var', 'xpt', 'session')
session_tags_dir = os.path.join(session_dir, 'tags')
cache_dir = os.path.join('var', 'xpt', 'cache')


def bytes_to_human(num, suffix='B'):
    for unit in ['','K','M','G','T','P','E','Z']:
        if abs(num) < 1024.0:
            return "%3.1f%s%s" % (num, unit, suffix)
        num /= 1024.0
    return "%.1f%s%s" % (num, 'Y', suffix)


class Session:
    def __init__(self):
        self.dict_package_url = dict()

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
                            f.write(url + '/' + tag + '/' + package + '\n')
        return 0

    def install(self, nargs):
        tag = nargs[-1] if len(nargs) > 2 and nargs[-2] == '@' else '_no_tag_'
        for url in open(os.path.join(session_tags_dir, tag)).read().splitlines():
            package = url.split('/')[-1].split('_')[0]
            if package in self.dict_package_url:
                print('*** ERROR: DUPLICATE NAME')
                print('**  Package 1: ' + url)
                print('*** Package 2: ' + self.dict_package_url.get(package))
                sys.exit(1)
            self.dict_package_url[package] = url
        packages = nargs[:-2] if len(nargs) > 2 and nargs[-2] == '@' else nargs
        for package in packages:
            if self.install_recursively(package) > 0:
                return 1
        return 0

    def install_recursively(self, package):
        # Скачиваем файл в packagename.zip_, после переименовываем в packagename.zip.
        # Так же в packagename.zip.info храним строку вида "url size".

        sys.stdout.write('Installing ' + package)
        if package not in self.dict_package_url.keys():
            print(' **** ERROR: PACKAGE NOT FOUND')
            return 1
        url = self.dict_package_url[package]
        sys.stdout.write(' ' + url)
        size = bytes_to_human(int(urllib.request.urlopen(url).info().get('Content-Length', -1)))
        sys.stdout.write(' ' + size)
        if not os.path.exists(cache_dir):
            os.makedirs(cache_dir)
        zip = os.path.join(cache_dir, url.split('/')[-1])
        info_size = None
        info_url = None
        saved_size = None
        if os.path.exists(zip+'.info'):
            info_size, info_url = open(zip + '.info').read().strip().split()
        if os.path.exists(zip):
            saved_size = os.path.getsize(zip)
        if info_url != url:
            if os.path.exists(zip):
                os.remove(zip)
            if os.path.exists(zip+'.info'):
                os.remove(zip)

        print('\n'+info_size+' '+info_url+' '+saved_size)
        #     print(saved_size + ' ' + saved_url)
        # if url == saved_url and size == saved_size:
        #     return 0
        # if url != saved_url:
        #     if os.path.exists()
        #
        # if os.path.exists(zip) and os.path.exists(zip+'.info'):
        #     print(saved_size +' '+ saved_url)
        #     if url != saved_url:
        #         os.remove(zip)
        #         os.remove(zip+'.info')


        sys.stdout.write('\n')
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

