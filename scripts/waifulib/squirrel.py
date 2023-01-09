# encoding: utf-8
# squirrel.py -- nut file support

from waflib import Errors, Logs, Task
from waflib.TaskGen import extension

@extension('nut')
def squirrel_hook(self, node):
    "Binds Squirrel file extensions to create :py:class:`waflib.Tools.Squirrel.cxx` instances"
    return self.create_compiled_task('nut', node)

class squirrel(Task.Task):
    """
    Squirrel support.

    Compile *.nut* files into *.cnut*::

        def configure(conf):
            conf.load('squirrel')
            conf.env.SQUIRRELDIR = '/usr/local/share/myapp/scripts/'
        def build(bld):
            bld(source='foo.nut')
    """
    color = 'BROWN'
    run_str = '${SQUIRREL} ${SRC}'
    ext_out = ['.cnut']

def configure(conf):
    # TODO(halotroop2288): This is a WIP. I've never dealt with Squirrel before,
    # never used Python, and I'm brand new to using Waf. So this is going to take some time.
    return
