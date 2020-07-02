#!/bin/bash
chmod +x $0
echo "                              Statistiche Clienti:"
awk '$1 == "[Cliente]"' log.txt | awk -F' ' '{ print "|id: " $3 " |buyProducts: " $5 "|supermarketTime: " $7 "|queueTime: " $9 "|changedQueue: " $11 "|"}'
echo ""
echo "                              Statistiche Cassieri:"
awk '$1 == "[Cassa]"' log.txt | awk -F' ' '{ print "|id: " $3 " |processedProducts: " $5 "|clientServed: " $7 "|openTime: " $9 "|avgTime: " $11 "|closures: " $13 "|"}'
