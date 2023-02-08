# import xml element tree
import xml.etree.ElementTree as ET
import datetime


# import mysql connector
import mysql.connector

# give the connection parameters
# user name is root
# password is empty
# server is localhost
# database name is database
jenkinsurl=(input(""))
time = datetime.datetime.utcnow()
conn = mysql.connector.connect(user='jenkins',
                               password='Polaris2022@',
                               host='100.20.0.129',
                               database='polaris')

cursorObject = conn.cursor()
tree = ET.parse(r'tata-project/unit-tests/build/ut_result.xml')
data2 = tree.findall('status')

# retrieving the data and insert into table
# i value for xml data #j value printing number of
# values that are stored
for i, j in zip(data2, range(1, 6)):
    name = i.find('name').text
    fail = i.find('fail').text
    success = i.find('pass').text
    #skip = i.find('skip').text
    tests =  i.find('tests').text
    #time = i.find('time').text
    # sql query to insert data into database
    data = """INSERT INTO unit_test_reports(name,tests,pass,fail,Jenkins_Job_Link,Timestamp) VALUES(%s,%s,%s,%s,%s,%s)"""
    # creating the cursor object
    c = conn.cursor()

    # executing cursor object
    c.execute(data, (name,tests,success,fail,jenkinsurl,time))
    conn.commit()
    #print("unit_test_reports student No-", j, " stored successfully")

