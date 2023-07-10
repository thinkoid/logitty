# Logitty

A console display manager.

# What it does

It launches an environment. When the login box is popping up (I do it in tty2) you will see name of the machine, underneath is an enum field that shows what you're starting, followed by your login and your password fields.

# Customization

Edit logitty.c, at the top there is an array that contains the details of the options you desire. 

Logitty is NOT trying to get smart and figure what you want. Anything you launch it better be smart enough to setup your stuff from a to z. I.e., if you start a Wayland compositor do it via a script that sets up its environment. Logitty is ONLY going to exec that for you.

The content of my logitty.c:

    ❯ git diff
    diff --git a/logitty.c b/logitty.c
    index bc30f24..88bc663 100644
    --- a/logitty.c
    +++ b/logitty.c
    @@ -29,7 +29,8 @@ static struct {
             char *argv[16];
     } startups[] = {
    -        { "shell", { "/bin/bash", 0 } }
    +        { "shell", { "/bin/bash", 0 } },
    +        { "dwl",   { "/usr/local/bin/run-dwl.sh", 0 }  }
     };

     static char *

Here I added an extra option to startup and now ```logitty``` offers one for launching the shell and the other for launching ```dwl```, a Wayland compositor. ```dwl``` is launched via the script indicated above which is a one-liner (ok, a two-liner) that simply execs the program.

    ❯ cat /usr/local/bin/run-dwl.sh
    #!/bin/zsh -l
    test x"$XDG_CONFIG_HOME" = x"" && export XDG_CONFIG_HOME="${HOME}/.config"
    exec dwl -s /usr/local/bin/dwl-startup.sh

Basically, it sets the environment variable ```XDG_CONFIG_HOME``` if not already set (it's not) before launching ```dwl``` with the startup instructions in ```dwl-startup.sh```:

    ❯ cat /usr/local/bin/dwl-startup.sh
    #!/bin/zsh

    # Gestures
    libinput-gestures-setup start

    # Audio via pipewire
    ps cax | grep pipewire &>/dev/null
    [ $? -eq 0 ] || { pipewire & }

    # ... other little programs I need, etc.

That's all.

# Launching

I only care about s6 support (with s6-rc, nonetheless).

Basically, after you do `make install`, you get a PAM file installed and the two service subdirs, `logitty-srv` and `logitty-log`. This means that now you have the service definition. Now, if you run artix, like me and other men of culture, you will need to add logitty to the list of services you want running by default so `touch /etc/s6/adminsv/default/contents.d/logitty`. Before recompiling the database though, remove tty2 from getty contents.d subdirectory: `rm etc/s6/sv/getty/contents.d/tty2`. Now you're ready to recompile the database. Do so, and at the next reboot, logitty will wait for your input on `tty2`.
