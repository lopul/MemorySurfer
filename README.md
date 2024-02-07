# MemorySurfer

A program (cgi-bin) to memorize (text- or HTML-based) flashcards. (Runs in a web browser on a web server).

## Screenshots

![slideshow](slideshow.gif)
![slideshow](slideshow-reveal.gif)
![Image](mathml.png "MathML")
![Image](xml.png "XML")

Some screenshots could be viewed
[here](https://www.lorenz-pullwitt.de/MemorySurfer/en/screenshots.html "screenshots").

## Try / Online / Live Demo

You can try MemorySurfer online without installing anything on your computer.
[Try MemorySurfer without installing anything](https://vps.lorenz-pullwitt.de/cgi-bin/memorysurfer.cgi).

Here is a (set of cards) <a href="https://www.lorenz-pullwitt.de/MemorySurfer/demo.xml" download>demo.xml</a> which can be used for Import (and testing).

## Installing MemorySurfer

The setup is described
[here](https://www.lorenz-pullwitt.de/MemorySurfer/en/setup.html "setup").

## Features

 - HTML or (and) TXT cards
 - XML export / import (for backup and version control tracking)
 - hierarchical decks
 - passphrase (access control of session / file)
 - searching

## Learning Algorithm

The learning algorithm is a simple spaced repetition algorithm, with a interesting variation: A deck with a lower "height" are prioritized. (leaf decks have a height of 0, the root deck has a height of the depth of the hierarchy tree). This allows to place the important cards in the leaf decks, to focus on them, and not get overwhelmed with too many cards to learn. This holds a efficient balance between cards actively remembered and cards which are only distantly remembered.

## Requirements

 - gcc
 - make
 - webserver

Every Linux system<sup>*</sup> capable of running a web-server (eg. Apache) should be capable of running MemorySurfer (tested on
[RasPiOS](https://www.lorenz-pullwitt.de/MemorySurfer/en/raspberry-pi-os.html "Raspberry Pi OS"),
[Fedora](https://www.lorenz-pullwitt.de/MemorySurfer/en/fedora.html "Fedora"),
[Debian](https://www.lorenz-pullwitt.de/MemorySurfer/en/debian.html "Debian"), Ubuntu).

<sup>*</sup> currently only little-endian architecture
