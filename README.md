# NAV429 Tester

Nav429 from [Naveol](http://www.naveol.com/) is an arduino shield that brings [ARINC429](https://fr.wikipedia.org/wiki/ARINC_429) connectivity to arduino boards.
It offers 2 x A429 inputs, 1 x A429 output and an [RS422](https://en.wikipedia.org/wiki/RS-422) RX/TX.
Optionally it can be fitted with a DC/DC converter that permits to power both Nav429 and its host CoM (computer on module) from the 28V power network of an aircraft.

The 3D view:
[HW](https://github.com/Art-ut-Kia/NAV-429-tester/wiki/Loop-back-cable) + SW necessary to perform a self-test of [Naveol](http://www.naveol.com/) Arduino Shield [NAV429](http://www.naveol.com/index.php?menu=product&p=3http://www.naveol.com/index.php?menu=product&p=3):

![](https://raw.githubusercontent.com/Art-ut-Kia/NAV-429-tester/master/WikiIllustrations/Nav429_board.png)

A picture of the bare board as delivered by Naveol:
<p align="center">
<img src="https://raw.githubusercontent.com/Art-ut-Kia/NAV-429-tester/master/WikiIllustrations/Nav429.jpg" width="300">
</p>

The same after installation of the Gaïa power supply module:
<p align="center">
<img src="https://raw.githubusercontent.com/Art-ut-Kia/NAV-429-tester/master/WikiIllustrations/Nav429_w_PwrSply.jpg" width="300">
</p>

## Supported boards

### arduino UNO
<p align="center">
<img src="https://raw.githubusercontent.com/Art-ut-Kia/NAV-429-tester/master/WikiIllustrations/ArduinoUno.png" width="300">
</p>

### nucleo (F746 & F767 tested)
<p align="center">
<img src="https://raw.githubusercontent.com/Art-ut-Kia/NAV-429-tester/master/WikiIllustrations/Nucleo.png" width="220">
</p>
