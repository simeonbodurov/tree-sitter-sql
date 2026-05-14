"""Integration test: parse Speedy.sql and count create_table nodes."""
import sys
import tree_sitter_sql as tssql
from tree_sitter import Language, Parser
from pathlib import Path

SPEEDY_SQL = Path(r'D:\Projects\Speedy NV\DataBase\Speedy.sql')
EXPECTED_TABLES = 218  # 214 CREATE TABLE + 4 CREATE GLOBAL TEMPORARY TABLE


def count_nodes(node, target):
    count = 1 if node.type == target else 0
    for child in node.children:
        count += count_nodes(child, target)
    return count


def collect_nodes(node, target, results=None):
    if results is None:
        results = []
    if node.type == target:
        results.append(node)
    for child in node.children:
        collect_nodes(child, target, results)
    return results


def main():
    print(f'tree_sitter_sql location: {tssql.__file__}')

    language = Language(tssql.language())
    parser = Parser(language)

    print(f'Reading {SPEEDY_SQL} ...')
    source = SPEEDY_SQL.read_bytes()
    print(f'  File size: {len(source):,} bytes')

    print('Parsing ...')
    tree = parser.parse(source)
    root = tree.root_node

    tables    = count_nodes(root, 'create_table')
    errors    = count_nodes(root, 'ERROR')
    set_terms = count_nodes(root, 'set_term')
    procs     = count_nodes(root, 'create_procedure')
    triggers  = count_nodes(root, 'create_trigger')
    domains   = count_nodes(root, 'create_domain')
    exceptions= count_nodes(root, 'create_exception')
    generators= count_nodes(root, 'create_generator')

    print()
    print('-' * 50)
    print(f'Root has_error : {root.has_error}')
    print(f'set_term nodes : {set_terms}')
    print(f'create_table   : {tables}  (expected {EXPECTED_TABLES})')
    print(f'create_procedure: {procs}')
    print(f'create_trigger : {triggers}')
    print(f'create_domain  : {domains}')
    print(f'create_exception: {exceptions}')
    print(f'create_generator: {generators}')
    print(f'ERROR nodes    : {errors}')
    print('-' * 50)

    if tables == EXPECTED_TABLES:
        print(f'PASS -- all {EXPECTED_TABLES} tables extracted')
    else:
        print(f'FAIL -- got {tables}, expected {EXPECTED_TABLES}')
        # Show which table names were found
        table_nodes = collect_nodes(root, 'create_table')
        print(f'\nTable names found ({len(table_nodes)}):')
        for tn in table_nodes:
            for child in tn.children:
                if child.type == 'object_reference':
                    name = source[child.start_byte:child.end_byte].decode('utf-8', errors='replace')
                    line = child.start_point[0] + 1
                    print(f'  L{line:6d}  {name}')
                    break

    return 0 if tables == EXPECTED_TABLES else 1


if __name__ == '__main__':
    sys.exit(main())
