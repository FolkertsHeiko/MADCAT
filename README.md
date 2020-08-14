This repository has been archived, because MADCAT v1 and v2 needed speration.

MADCAT v1 maintenance is now in seperate repository:
https://github.com/FolkertsHeiko/MADCAT-v1

MADCAT v2 maintenance is now in seperate repository, too:
https://github.com/FolkertsHeiko/MADCAT-v2


Contact:
heiko.folkerts [at] bsi.bund.de or github [at] thevoid.email

                             ▄▄▄               ▄▄▄▄▄▄
                 ▀▄▄      ▄▓▓█▓▓▓█▌           ██▓██▓▓██▄     ▄▀
                    ▀▄▄▄▓█▓██   █▓█▌         █▓   ▓████████▀
                       ▀███▓▓(o)██▓▌       ▐█▓█(o)█▓█████▀
                         ▀▀██▓█▓▓█         ████▓███▀▀
                  ▄            ▀▀▀▀                          ▄
                ▀▀█                                         ▐██▌
                  ██▄     ____------▐██████▌------___     ▄▄██
                 __█ █▄▄--   ___------▀▓▓▀-----___   --▄▄█ █▀__
             __--   ▀█  ██▄▄▄▄    __--▄▓▓▄--__   ▄▄▄▄██  ██▀   --__
         __--     __--▀█ ██  █▀▀█████▄▄▄▄▄▄███████  ██ █▀--__      --__
     __--     __--    __▀▀█  █  ██  ██▀▀██▀▀██  ██  █▀▀__    --__      --__
         __--     __--     ▀███ ██  ██  ██  ██ ████▀     --__    --__
      --      __--             ▀▀▀▀▀██▄▄██▄▄██▀▀▀▀           --__    -- hfo
         __ --                                                   --__

In recent years, mass attacks on Internet users have steadily increased. Again and again new methods and patterns of attack come to light. One example was a major incident that affected 900,000 connections in Germany in 2016. However, many of these attempted attacks remain undetected or are not reported, because they do not lead to a correspondingly large impairment. Therefore, it is necessary to collect data on attacks and attempted attacks on the Internet. A well-known and widespread means of detecting such attempted attacks are honeypots. In addition, it is also possible to collect data on the longer-term, temporal development of mass attacks and thus make better forecasts of future developments.
       For this purpose MADCAT (Mass Attack Detection Connection Acceptance Tools) has been developed as a universal, honeypot-like cyber-threaded detetecion suite with low interaction. A honeypot is a server that simulates or emulates common network services. Honeypots are used to obtain information about attack patterns and attacker behavior. If such a honeypot service is accessed, the corresponding actions are recorded and an alarm is triggered if necessary. The idea behind the operation of honeypot systems is to offer one or more services that are not in productive use by users and therefore cannot be found and accessed during normal operation. An attacker who searches for vulnerabilities in network components and cannot distinguish between real servers and honeypots will therefore probably be registered by a honeypot. Usually at least the IP address of the attacker, the time of the attack and the actions of the attacker are logged like used access data. Furthermore, in the case of simulated web forms or command lines and other services the inputs may be recorded in order to be able to trace the attempted attack.
       MADCAT is similar to a honeypot, because it records all contact attempts without being limited to certain services. Low interaction indicates that in the current version of MADCAT v1 answers to contact attempts are only given to the extent that is technically absolutely necessary to establish a connection between potential attacker and sensor in order to detect attack vectors if feasible. For MADCAT v2 a higher level of interaction with attackers is planned, e.g. to register unauthorized username/password combinations used by them.
