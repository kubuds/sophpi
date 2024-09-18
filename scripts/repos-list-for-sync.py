#!/usr/bin/env python

import xml.etree.ElementTree as ET
import json
import requests
import re
import os
import sys


scriptdir = os.path.dirname(os.path.abspath(__file__))
tree = ET.parse(f"{scriptdir}/subtree.xml")
root = tree.getroot()

exit_code = 0
branch_default = ''
remote_default = ''
local_owner = os.getenv('GITHUB_LOCAL_REPO_OWNER')
if local_owner is None:
    local_owner = 'kubuds'
repos = []
remotes = {}
repos_need_to_be_sync = []
github_token = os.getenv('GITHUB_TOKEN')
if github_token is None:
    print("Error: 'GITHUB_TOKEN' environment variable is not set.")
    sys.exit(1)


for elem in root:
    if elem.tag == 'remote':
        attr = elem.attrib
        if 'name' in attr and 'fetch' in attr:
            remotes[attr['name']] = attr['fetch']
    elif elem.tag == 'default':
        branch_default = elem.attrib['revision']
        remote_default = elem.attrib['remote']
    elif elem.tag == 'project':
        attr = elem.attrib
        repos.append({'name': attr['name']})
        if 'revision' in attr:
            repos[-1]['branch'] = attr['revision']
        if 'remote' in attr:
            repos[-1]['remote'] = attr['remote']


def https_ver_of_remote_url(url):
    pattern_git = r'^git@github\.com:(.*)$'
    matched = re.match(pattern_git, url)
    if matched:
        return "https://github.com/%s" % matched.groups()[0]
    else:
        return url


def owner_from_remote_url(url):
    pattern_github = r'github\.com(:|/)([^/]+)'
    matched = re.search(pattern_github, url)
    if matched:
        return matched.groups()[1]
    else:
        return 'sophgo' # the default owner


def branch_exists(owner, repo, branch):
    is_public = False
    url = f"https://api.github.com/repos/{owner}/{repo}/branches/{branch}"
    url4repo = f"https://api.github.com/repos/{owner}/{repo}"
    headers = {
        "Authorization": f"token {github_token}"
    }
    response = requests.get(url, headers=headers)
    response4repo = requests.get(url4repo, headers=headers)
    # TODO, how to request only once to get the branch status and check the repo
    # is public or not?
    
    if response.status_code == 200:
        repo_data = response4repo.json()
        if not repo_data.get("private", True):
            is_public = True
        return True, is_public
    elif response.status_code == 404:
        return False, False
    else:
        # TODO: handle other status codes or errors
        raise Exception(f"Error checking repo: {response.status_code}")


for repo in repos:
    repo_name = repo['name']
    repo_branch = branch_default
    if 'branch' in repo:
        repo_branch = repo['branch']
    repo_remote = remote_default
    if 'remote' in repo:
        repo_remote = repo['remote']
    remote_url = remotes[repo_remote]
    if not remote_url.endswith('/'):
        remote_url += '/'

    # the default remote url is git@ version, we should switch to https version
    # to satisfy Github Action
    remote_url_https = https_ver_of_remote_url(remote_url)
    repos_need_to_be_sync.append({
        'name': repo_name,
        'branch': repo_branch,
        'local': f"{local_owner}/{repo_name}",
        'remote': {'name': repo_remote, 'url': f"{remote_url_https}{repo_name}"},
        'status': 'undefined'
    })

    repo_exist, repo_is_public = branch_exists(
        owner_from_remote_url(remote_url),
        repo_name, repo_branch
    )
    if repo_exist and repo_is_public:
        print(f"\x1b[32m{repo_name} with branch {repo_branch} exists on remote {remote_url}\x1b[0m")
        k_repo_exist, k_repo_is_public = branch_exists(local_owner, repo_name, repo_branch)
        if k_repo_exist and k_repo_is_public:
            print(f"\x1b[32mKubuds: {repo_name} with branch {repo_branch} exists\x1b[0m")
            repos_need_to_be_sync[-1]['status'] = 'good'
        elif k_repo_exist:
            print(f"\x1b[31mKubuds: {repo_name} with branch {repo_branch} exists but is not public\x1b[0m", file=sys.stderr)
            exit_code = 1
        else:
            print(f"\x1b[31mKubuds: {repo_name} with branch {repo_branch} doesn't exist\x1b[0m", file=sys.stderr)
            exit_code = 1
    elif repo_exist:
        print(f"\x1b[31m{repo_name} with branch {repo_branch} exists on remote {remote_url} but is not public\x1b[0m", file=sys.stderr)
        exit_code = 1
    else:
        print(f"\x1b[31m{repo_name} with branch {repo_branch} doesn't exist on remote {remote_url}\x1b[0m", file=sys.stderr)
        exit_code = 1


jsonlized_repos = json.dumps(repos_need_to_be_sync)
print("list={\"repo\": %s}" % jsonlized_repos)

github_output = os.getenv('GITHUB_OUTPUT')
if github_output is not None:
    with open(github_output, 'a') as f:
        f.write("list={\"repo\": %s}\n" % jsonlized_repos)

sys.exit(exit_code)

