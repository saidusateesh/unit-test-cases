import read_xml as inp
import xml.etree.ElementTree as ET
pas = str(inp.success)
fail = str(inp.failures)
#skip = str(inp.skipped)
err = str(inp.errors)
tests=str(inp.total_testcases)
filename = r'tata-project/unit-tests/build/ut_result.xml'
xmlTree = ET.parse(filename)
rootElement = xmlTree.getroot()
#xml_data = open(r'C:\Users\30370\Downloads\sample_result.xml').read()  # Read file
for root_val in rootElement.iter('status'):
     if root_val.find('name').text=="AllTests":
          root_val.find('pass').text = pas
          root_val.find('fail').text = fail
          #root_val.find('skip').text = "0"
          root_val.find('error').text = err
          root_val.find('tests').text = tests


#print(rank)
xmlTree.write(r'tata-project/unit-tests/build/ut_result.xml',encoding='UTF-8',xml_declaration=True)
