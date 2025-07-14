import { useContext, useEffect, useState } from "react";
import { StateStoreContext } from "../state/State";
import { observer } from "mobx-react";
import { IExpressionStore, math } from "$src/document/ExpressionStore"
import { VscodeTextfield } from "@vscode-elements/elements";
import "@vscode-elements/elements/dist/vscode-table";
import "@vscode-elements/elements/dist/vscode-table-body";
import "@vscode-elements/elements/dist/vscode-table-row";
import "@vscode-elements/elements/dist/vscode-table-header";
import "@vscode-elements/elements/dist/vscode-table-header-cell";
import "@vscode-elements/elements/dist/vscode-table-cell";
import "@vscode-elements/elements/dist/vscode-textfield";
import { autorun } from "mobx";

type VSCodeExpressionInputProps = {
    expr: IExpressionStore
}
const VSCodeExpressionInput = observer((props: VSCodeExpressionInputProps) => {
    
    const [editedValue, setEditedValue] = useState(props.expr.expr.toString());
    const [errors, setErrors] = useState(false);
    const handleChange = (event:any) => {
        
        let valid = props.expr.validate(
              math.parse(event.target.value)
            )!== undefined;
        console.log(valid, event.target.value);
        setErrors(!valid);
        setEditedValue(event.target.value);
    };
    const handleSubmit = (event:any) => {
        console.log(event);
        const newNode = props.expr.validate(
            math.parse(event.target.value)
        );
        if (newNode !== undefined && !newNode.equals(props.expr.expr)) {
            props.expr.set(newNode);
            console.log(newNode.toString());
        } else {
            setEditedValue(props.expr.expr.toString());
            event.target.blur();
            setErrors(!props.expr.valid);
        }
    }
    useEffect(()=>autorun(()=>setEditedValue(props.expr.expr.toString())), []);

    return (<vscode-textfield onchange={handleSubmit} onInput={handleChange}
        value={editedValue}
        invalid={errors}
    >

    </vscode-textfield>)
})

type RobotConfigRowProps = {
    title: string,
    expr: IExpressionStore,
}
const RobotConfigRow = observer((props: RobotConfigRowProps) => (
    <vscode-table-row>
        <vscode-table-cell>{props.title}</vscode-table-cell>
        <vscode-table-cell>
            <VSCodeExpressionInput expr={props.expr}></VSCodeExpressionInput>
        </vscode-table-cell>
    </vscode-table-row>
))
const RobotConfigView = observer(() => {
    const stateStore = useContext(StateStoreContext)!;
    const robotConfig = stateStore.config;
    return (
        <>
            <vscode-table zebra bordered-rows resizable>
                <vscode-table-body slot="body">
                    <RobotConfigRow title="Mass" expr={robotConfig.mass}></RobotConfigRow>
                </vscode-table-body>
            </vscode-table>
        </>
    )
})

export default RobotConfigView;