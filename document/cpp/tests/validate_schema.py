import argparse
import json
from pathlib import Path

from jsonschema import Draft202012Validator
from referencing import Registry, Resource


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("build_directory", type=Path)
    args = parser.parse_args()
    schemas_directory = Path(__file__).resolve().parents[2]
    schemas = {}
    registry = Registry()
    for path in schemas_directory.glob("*.schema.json"):
        schema = json.loads(path.read_text(encoding="utf-8"))
        Draft202012Validator.check_schema(schema)
        schemas[path.name] = schema
        registry = registry.with_resource(schema["$id"], Resource.from_contents(schema))
    for schema_name, document_name in (
        ("project.schema.json", "schema-project.json"),
        ("trajectory.schema.json", "schema-trajectory.json"),
    ):
        document = json.loads((args.build_directory / document_name).read_text(encoding="utf-8"))
        Draft202012Validator(schemas[schema_name], registry=registry).validate(document)
    print("Serialized project and trajectory conform to their schemas")


if __name__ == "__main__":
    main()