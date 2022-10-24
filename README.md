# Logitty

A console display manager.

# What it does

It launches the environment/shell/graphical craddle that you care for. When the login box is popping up (I do it in tty2) you get the name of the machine, underneath is an enum field that shows what you're starting, followed by your login and your password fields.

# Customization

Edit logitty.c, at the top there is an array that contains the details of the options you desire. If your C skillz are worth anything you'll figure it out. Basically, it's a label followed by an execv array that is used to launch your environment.

Logitty is NOT trying to get smart and figure what you want. Anything you launch it better be smart enough to setup your stuff from a to z. I.e., if you start a Wayland compositor do it via a script that sets up its environment. Logitty is ONLY going to launch that shit for you.

# Launching

I only care about s6 support (with s6-rc, nonetheless).

Basically, after you do `make install`, you get a PAM file installed and the two service subdirs, `logitty-srv` and `logitty-log`. This means that now you have the service definition. Now, if you run artix, like me and other men of culture, you will need to add logitty to the list of services you want running by default so `touch /etc/s6/adminsv/default/contents.d/logitty`. Before recompiling the database though, remove tty2 from getty contents.d subdirectory: `rm etc/s6/sv/getty/contents.d/tty2`. Now you're ready to recompile the database. Do so, and at the next reboot, logitty will wait for your input on `tty2`.
