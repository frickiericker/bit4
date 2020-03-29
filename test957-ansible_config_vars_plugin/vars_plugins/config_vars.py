import os

from ansible.plugins.vars import BaseVarsPlugin
from ansible.utils.vars import merge_hash


# Ansible <= 2.9 does not pass options to vars plugins.
DOCUMENTATION = """
vars: config_vars
short_description: Merge variables from files in config directory
options:
  dirname:
    type: str
    ini:
      - key: path
        section: config_vars
    env:
      - name: ANSIBLE_CONFIG_VARS_PATH
"""

DEFAULT_PATH = "config"


class VarsModule(BaseVarsPlugin):
    def get_vars(self, loader, path, entities, cache=True):
        super().get_vars(loader, path, entities)

        # TODO: Use get_option once Ansible 2.10 is released.
        config_path = os.getenv("ANSIBLE_CONFIG_VARS_PATH") or DEFAULT_PATH
        vars_files = loader.find_vars_files(self._basedir, config_path)

        merged_vars = {}
        for file_path in sorted(vars_files):
            loaded_vars = loader.load_from_file(file_path)
            merged_vars = merge_hash(merged_vars, loaded_vars)

        return merged_vars
